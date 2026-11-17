#pragma once
#include "core/MeshData.h"
#include "world/HotelMap.h"

namespace hotel {

struct RoomStyle {
    float wallHeight = 2.6f;
    float doorHeight = 2.0f;
    Color floorA{0.80f, 0.72f, 0.60f, 1};
    Color floorB{0.70f, 0.62f, 0.50f, 1};
    Color doorFloor{0.45f, 0.30f, 0.20f, 1};
    Color wall{0.94f, 0.91f, 0.85f, 1};
    Color lintel{0.55f, 0.38f, 0.25f, 1};
};

// Procedural Room Builder: HotelMap -> MeshData (lantai, dinding, ambang pintu).
// Sisi dinding yang tidak terlihat (antar-dinding / sisi luar) tidak dibuat.
class RoomBuilder {
public:
    static MeshData build(const HotelMap& map, const RoomStyle& style = RoomStyle{});
};

} // namespace hotel
