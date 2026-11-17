#pragma once
#include <string>
#include <vector>
#include "furniture/FurnitureCatalog.h"
#include "world/HotelMap.h"

namespace hotel {

struct PlacedFurniture {
    int uid;
    int defId;
    int x, y;     // pojok kiri-atas footprint setelah dirotasi
    int rot;      // 0..3 (x90 derajat, searah Mat4::rotationY)
};

// Mengelola furnitur di grid: validasi, penempatan, penghapusan, dan memblokir tile di HotelMap.
class FurnitureManager {
public:
    void bind(HotelMap* map) { map_ = map; }
    void clear();

    static void footprintSize(const FurnitureDef& d, int rot, int& w, int& h);
    static IVec2 facing(int rot);   // arah depan dalam tile: rot0 = +Z

    // reason (opsional) diisi alasan penolakan dalam bahasa Indonesia, huruf besar.
    bool canPlace(int defId, int x, int y, int rot, IVec2 playerTile, std::string* reason = nullptr);
    int place(int defId, int x, int y, int rot);   // uid atau -1
    bool remove(int uid);

    const std::vector<PlacedFurniture>& items() const { return items_; }
    const PlacedFurniture* byUid(int uid) const;
    const PlacedFurniture* at(int tx, int ty) const;

    Vec3 centerOf(const PlacedFurniture& p) const;
    std::vector<IVec2> frontTiles(const PlacedFurniture& p) const;
    // Tile berdiri di sekitar furnitur (utamakan sisi depan) yang bisa dilewati & terjangkau dari pintu masuk.
    bool standTile(const PlacedFurniture& p, IVec2& out) const;
    int regionOf(const PlacedFurniture& p) const { return map_->regionAt(p.x, p.y); }
    int countDecorInRegion(int region) const;
    bool hasType(FurnType t) const;

    // Simpan/muat: satu baris per furnitur "F defId x y rot".
    std::string serialize() const;
    void loadLine(const std::string& line);

private:
    void setBlockedFor(const PlacedFurniture& p, bool b);
    bool connectivityOK() const;

    HotelMap* map_ = nullptr;
    std::vector<PlacedFurniture> items_;
    int nextUid_ = 1;
};

} // namespace hotel
