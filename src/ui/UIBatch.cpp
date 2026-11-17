#include "ui/UIBatch.h"
#include "ui/Font.h"

namespace hotel {

void UIBatch::tri(float ax, float ay, float bx, float by, float cx, float cy, Color c) {
    verts_.push_back({ax, ay, c.r, c.g, c.b, c.a});
    verts_.push_back({bx, by, c.r, c.g, c.b, c.a});
    verts_.push_back({cx, cy, c.r, c.g, c.b, c.a});
}

void UIBatch::addRect(float x, float y, float w, float h, Color c) {
    tri(x, y, x + w, y, x + w, y + h, c);
    tri(x, y, x + w, y + h, x, y + h, c);
}

void UIBatch::addCircle(float cx, float cy, float r, Color c, int seg) {
    for (int i = 0; i < seg; ++i) {
        float a0 = 2.0f * kPi * i / seg, a1 = 2.0f * kPi * (i + 1) / seg;
        tri(cx, cy, cx + r * std::cos(a0), cy + r * std::sin(a0),
                    cx + r * std::cos(a1), cy + r * std::sin(a1), c);
    }
}

void UIBatch::addRing(float cx, float cy, float r, float t, Color c, int seg) {
    float ri = r - t;
    for (int i = 0; i < seg; ++i) {
        float a0 = 2.0f * kPi * i / seg, a1 = 2.0f * kPi * (i + 1) / seg;
        float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
        tri(cx + r * c0, cy + r * s0, cx + r * c1, cy + r * s1, cx + ri * c1, cy + ri * s1, c);
        tri(cx + r * c0, cy + r * s0, cx + ri * c1, cy + ri * s1, cx + ri * c0, cy + ri * s0, c);
    }
}

void UIBatch::addText(float x, float y, float scale, const std::string& text, Color c) {
    float penX = x;
    for (char ch : text) {
        const unsigned char* g = Font::glyph(ch);
        for (int row = 0; row < Font::kGlyphH; ++row) {
            int col = 0;
            while (col < Font::kGlyphW) {
                if (!(g[row] & (0x10 >> col))) { ++col; continue; }
                int start = col;
                while (col < Font::kGlyphW && (g[row] & (0x10 >> col))) ++col;
                addRect(penX + start * scale, y + row * scale, (col - start) * scale, scale, c);
            }
        }
        penX += Font::kAdvance * scale;
    }
}

void UIBatch::addTextCentered(float cx, float cy, float scale, const std::string& text, Color c) {
    addText(cx - Font::textWidth(text, scale) * 0.5f, cy - Font::textHeight(scale) * 0.5f, scale, text, c);
}

} // namespace hotel
