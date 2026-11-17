#include "ui/TouchUI.h"
#include <algorithm>
#include "ui/Font.h"

namespace hotel {

void TouchUI::setScreenSize(float w, float h) {
    screenW_ = w;
    screenH_ = h;
    radius_ = 0.14f * h;
}

void TouchUI::setLayout(std::vector<UIButton> buttons, std::vector<UIRect> panels) {
    buttons_ = std::move(buttons);
    panels_ = std::move(panels);
}

bool TouchUI::hitPanel(float x, float y) const {
    for (const auto& p : panels_) if (p.contains(x, y)) return true;
    return false;
}

const UIButton* TouchUI::findButton(int id) const {
    for (const auto& b : buttons_) if (b.id == id) return &b;
    return nullptr;
}

void TouchUI::onTouch(TouchAction a, int id, float x, float y) {
    auto slot = [&](int pid) {
        for (int i = 0; i < pressedCount_; ++i) if (pressedPointers_[i] == pid) return i;
        return -1;
    };
    auto drop = [&](int i) {
        pressedPointers_[i] = pressedPointers_[pressedCount_ - 1];
        pressedButton_[i] = pressedButton_[pressedCount_ - 1];
        --pressedCount_;
    };

    switch (a) {
    case TouchAction::Down: {
        for (const auto& b : buttons_) {
            if (b.enabled && b.rect.contains(x, y) && pressedCount_ < kMaxPointers) {
                pressedPointers_[pressedCount_] = id;
                pressedButton_[pressedCount_] = b.id;
                ++pressedCount_;
                return;
            }
        }
        if (hitPanel(x, y)) return;                     // panel menelan sentuhan
        if (!joyActive_ && x < screenW_ * 0.5f) {
            joyActive_ = true; joyId_ = id;
            joyOrigin_ = joyPos_ = {x, y};
            updateAxis();
        } else if (camId_ < 0 && x >= screenW_ * 0.5f) {
            camId_ = id; camLast_ = {x, y};
        }
        break;
    }
    case TouchAction::Move:
        if (joyActive_ && id == joyId_) {
            joyPos_ = {x, y};
            updateAxis();
        } else if (id == camId_) {
            camDelta_.x += x - camLast_.x;
            camDelta_.y += y - camLast_.y;
            camLast_ = {x, y};
        }
        break;
    case TouchAction::Up:
    case TouchAction::Cancel: {
        int s = slot(id);
        if (s >= 0) {
            if (a == TouchAction::Up) {
                const UIButton* b = findButton(pressedButton_[s]);
                if (b && b->enabled && b->rect.contains(x, y)) clicks_.push_back(b->id);
            }
            drop(s);
        }
        if (joyActive_ && id == joyId_) { joyActive_ = false; joyId_ = -1; axis_ = {}; }
        if (id == camId_) camId_ = -1;
        break;
    }
    }
}

void TouchUI::updateAxis() {
    float dx = (joyPos_.x - joyOrigin_.x) / radius_;
    float dy = -(joyPos_.y - joyOrigin_.y) / radius_;
    float len = std::sqrt(dx * dx + dy * dy);
    const float dead = 0.12f;
    if (len < dead) { axis_ = {}; return; }
    float scaled = std::min(1.0f, (len - dead) / (1.0f - dead));
    axis_ = {dx / len * scaled, dy / len * scaled};
}

Vec2 TouchUI::consumeCameraDelta() {
    Vec2 d = camDelta_;
    camDelta_ = {};
    return d;
}

std::vector<int> TouchUI::consumeClicks() {
    std::vector<int> out;
    out.swap(clicks_);
    return out;
}

void TouchUI::draw(UIBatch& b) const {
    // panel
    for (const auto& p : panels_) {
        b.addRect(p.x, p.y, p.w, p.h, {0.05f, 0.07f, 0.12f, 0.82f});
        float t = 3.0f;
        b.addRect(p.x, p.y, p.w, t, {1, 1, 1, 0.35f});
        b.addRect(p.x, p.y + p.h - t, p.w, t, {1, 1, 1, 0.35f});
        b.addRect(p.x, p.y, t, p.h, {1, 1, 1, 0.35f});
        b.addRect(p.x + p.w - t, p.y, t, p.h, {1, 1, 1, 0.35f});
    }

    // tombol
    for (const auto& btn : buttons_) {
        bool pressed = false;
        for (int i = 0; i < pressedCount_; ++i) if (pressedButton_[i] == btn.id) pressed = true;
        Color c = btn.color;
        if (!btn.enabled) c = {0.35f, 0.35f, 0.38f, 0.85f};
        else if (pressed) c = {c.r * 0.7f, c.g * 0.7f, c.b * 0.7f, c.a};
        const UIRect& r = btn.rect;
        b.addRect(r.x, r.y, r.w, r.h, c);
        float t = std::max(2.0f, btn.textScale * 0.7f);
        Color edge{1, 1, 1, 0.55f};
        b.addRect(r.x, r.y, r.w, t, edge);
        b.addRect(r.x, r.y + r.h - t, r.w, t, edge);
        b.addRect(r.x, r.y, t, r.h, edge);
        b.addRect(r.x + r.w - t, r.y, t, r.h, edge);

        float cy = r.y + r.h * 0.5f;
        if (btn.sub.empty()) {
            b.addTextCentered(r.x + r.w * 0.5f, cy, btn.textScale, btn.label, {1, 1, 1, 1});
        } else {
            b.addTextCentered(r.x + r.w * 0.5f, cy - btn.textScale * 4.5f, btn.textScale, btn.label, {1, 1, 1, 1});
            b.addTextCentered(r.x + r.w * 0.5f, cy + btn.textScale * 4.5f, btn.textScale, btn.sub, {1.0f, 0.92f, 0.45f, 1});
        }
    }

    // joystick
    float cx, cy, kx, ky, alpha;
    if (joyActive_) {
        cx = joyOrigin_.x; cy = joyOrigin_.y;
        float dx = joyPos_.x - cx, dy = joyPos_.y - cy;
        float len = std::sqrt(dx * dx + dy * dy);
        float k = len > radius_ ? radius_ / len : 1.0f;
        kx = cx + dx * k; ky = cy + dy * k;
        alpha = 0.55f;
    } else {
        cx = kx = 0.16f * screenW_;
        cy = ky = 0.74f * screenH_;
        alpha = 0.22f;
    }
    b.addCircle(cx, cy, radius_, {1, 1, 1, alpha * 0.35f});
    b.addRing(cx, cy, radius_, 3.0f, {1, 1, 1, alpha});
    b.addCircle(kx, ky, radius_ * 0.4f, {1, 1, 1, alpha + 0.2f});
}

} // namespace hotel
