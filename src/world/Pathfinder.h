#pragma once
#include <vector>
#include "world/HotelMap.h"

namespace hotel {

// Pathfinding grid 4 arah (A*). Dipakai GuestAI dan validasi penempatan furnitur.
class Pathfinder {
public:
    // Mengisi 'out' dengan tile dari langkah pertama sampai tujuan (start tidak ikut).
    // true jika ada jalan (out kosong jika start == goal). Tujuan harus walkable.
    static bool findPath(const HotelMap& map, IVec2 start, IVec2 goal, std::vector<IVec2>& out);

    // Flood fill dari start lewat tile walkable. Hasil: peta width*height berisi 0/1.
    static std::vector<uint8_t> reachable(const HotelMap& map, IVec2 start);
};

} // namespace hotel
