#pragma once
#include <vector>
#include <string>
#include "core/Math.h"

namespace hotel {

// Koordinat piksel layar, origin kiri-atas. Tanpa dependensi GL.
struct UIVertex { float x, y, r, g, b, a; };

class UIBatch {
public:
    void clear() { verts_.clear(); }
    void addRect(float x, float y, float w, float h, Color c);
    void addCircle(float cx, float cy, float r, Color c, int segments = 32);
    void addRing(float cx, float cy, float r, float thickness, Color c, int segments = 48);
    // Teks font bitmap; (x, y) = pojok kiri-atas. Run piksel mendatar digabung agar hemat vertex.
    void addText(float x, float y, float scale, const std::string& text, Color c);
    void addTextCentered(float cx, float cy, float scale, const std::string& text, Color c);
    const std::vector<UIVertex>& vertices() const { return verts_; }

private:
    void tri(float ax, float ay, float bx, float by, float cx, float cy, Color c);
    std::vector<UIVertex> verts_;
};

} // namespace hotel
