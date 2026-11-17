#include "world/RoomBuilder.h"

namespace hotel {

namespace {
struct Dir { int dx, dy; uint8_t face; };
constexpr Dir kDirs[4] = {
    { 1,  0, MeshData::PosX},
    {-1,  0, MeshData::NegX},
    { 0,  1, MeshData::PosZ},
    { 0, -1, MeshData::NegZ},
};
}

MeshData RoomBuilder::build(const HotelMap& map, const RoomStyle& s) {
    MeshData mesh;
    const float ts = map.tileSize();

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const Tile t = map.at(x, y);
            const Vec3 lo{x * ts, 0.0f, y * ts};
            const Vec3 hi{(x + 1) * ts, 0.0f, (y + 1) * ts};

            if (t == Tile::Floor || t == Tile::Door) {
                Color c = (t == Tile::Door) ? s.doorFloor : (((x + y) & 1) ? s.floorA : s.floorB);
                mesh.addBox(lo, hi, c, MeshData::PosY);   // satu quad di y = 0
            }

            if (t == Tile::Wall) {
                uint8_t faces = MeshData::PosY;
                for (const Dir& d : kDirs)
                    if (map.isWalkable(x + d.dx, y + d.dy)) faces |= d.face;
                mesh.addBox({lo.x, 0, lo.z}, {hi.x, s.wallHeight, hi.z}, s.wall, faces);
            }

            if (t == Tile::Door) {  // ambang di atas pintu
                uint8_t faces = MeshData::PosY | MeshData::NegY;
                for (const Dir& d : kDirs)
                    if (map.at(x + d.dx, y + d.dy) != Tile::Wall) faces |= d.face;
                mesh.addBox({lo.x, s.doorHeight, lo.z}, {hi.x, s.wallHeight, hi.z}, s.lintel, faces);
            }
        }
    }
    return mesh;
}

} // namespace hotel
