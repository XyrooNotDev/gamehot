#pragma once
#include <cstdint>
#include <vector>
#include "core/Math.h"

namespace hotel {

// Tekstur CPU. Dipakai untuk "membakar" warna tekstur ke warna vertex saat model dimuat,
// jadi renderer tidak perlu pipeline tekstur (cocok untuk model low-poly palet warna ala Kenney/Quaternius).
struct Texture {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    bool valid() const { return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4; }
    Color sample(float u, float v) const;   // nearest, wrap; v=0 di atas gambar (konvensi glTF)
};

// PNG decoder (8-bit & palet 1/2/4/8, tanpa interlace). Bawaan: inflate sendiri.
bool decodePng(const uint8_t* data, size_t size, Texture& out);
// zlib inflate (dipakai PNG); dibuka untuk tes.
bool zlibInflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out);

} // namespace hotel
