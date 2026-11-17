#pragma once
#include <string>
#include <vector>
#include "core/Math.h"
#include "ui/UIBatch.h"

namespace hotel {

enum class TouchAction { Down, Move, Up, Cancel };

struct UIRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct UIButton {
    int id = 0;
    UIRect rect;
    std::string label;
    std::string sub;           // baris kecil (mis. harga)
    Color color{0.20f, 0.45f, 0.80f, 0.90f};
    bool enabled = true;
    float textScale = 3.0f;
};

// Lapisan UI sentuh: joystick melayang (separuh kiri), drag kamera (separuh kanan),
// tombol & panel. Layout diganti Game tiap frame lewat setLayout().
class TouchUI {
public:
    void setScreenSize(float w, float h);
    void setLayout(std::vector<UIButton> buttons, std::vector<UIRect> panels);
    void onTouch(TouchAction action, int pointerId, float x, float y);

    Vec2 moveAxis() const { return axis_; }
    Vec2 consumeCameraDelta();
    std::vector<int> consumeClicks();
    void injectClick(int id) { clicks_.push_back(id); }   // untuk tes
    const std::vector<UIButton>& buttons() const { return buttons_; }

    void draw(UIBatch& batch) const;

private:
    static constexpr int kMaxPointers = 16;
    bool hitPanel(float x, float y) const;
    const UIButton* findButton(int id) const;
    void updateAxis();

    float screenW_ = 1, screenH_ = 1, radius_ = 100;
    std::vector<UIButton> buttons_;
    std::vector<UIRect> panels_;
    std::vector<int> clicks_;

    int pressedButton_[kMaxPointers];    // id tombol per pointer, -1 = tidak ada
    int pressedPointers_[kMaxPointers];
    int pressedCount_ = 0;

    bool joyActive_ = false;
    int joyId_ = -1;
    Vec2 joyOrigin_, joyPos_, axis_;
    int camId_ = -1;
    Vec2 camLast_, camDelta_;
};

} // namespace hotel
