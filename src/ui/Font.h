#pragma once
#include <string>

namespace hotel {

// Font bitmap 5x7 (huruf besar, angka, beberapa tanda baca). Tanpa file font / library.
struct Font {
    static constexpr int kGlyphW = 5, kGlyphH = 7, kAdvance = 6;
    // Baris-baris glyph (bit 4 = piksel paling kiri). Karakter tak dikenal -> kotak kosong.
    static const unsigned char* glyph(char c);
    static float textWidth(const std::string& s, float scale) {
        return s.empty() ? 0.0f : (s.size() * kAdvance - 1) * scale;
    }
    static float textHeight(float scale) { return kGlyphH * scale; }
};

} // namespace hotel
