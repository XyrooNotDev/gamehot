#include "furniture/FurnitureManager.h"
#include <algorithm>
#include <sstream>
#include "world/Pathfinder.h"

namespace hotel {

void FurnitureManager::clear() {
    for (auto& p : items_) setBlockedFor(p, false);
    items_.clear();
    nextUid_ = 1;
}

void FurnitureManager::footprintSize(const FurnitureDef& d, int rot, int& w, int& h) {
    bool odd = (rot & 1) != 0;
    w = odd ? d.h : d.w;
    h = odd ? d.w : d.h;
}

IVec2 FurnitureManager::facing(int rot) {
    static const IVec2 kF[4] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    return kF[rot & 3];
}

void FurnitureManager::setBlockedFor(const PlacedFurniture& p, bool b) {
    const FurnitureDef* d = findFurniture(p.defId);
    if (!d || !map_) return;
    int w, h;
    footprintSize(*d, p.rot, w, h);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) map_->setBlocked(p.x + i, p.y + j, b);
}

bool FurnitureManager::connectivityOK() const {
    if (!map_->hasEntrance()) return true;
    auto seen = Pathfinder::reachable(*map_, map_->entrance());
    const int W = map_->width();
    auto vis = [&](int x, int y) { return map_->isWalkable(x, y) && seen[y * W + x]; };

    for (int y = 0; y < map_->height(); ++y)      // semua pintu harus tetap terjangkau
        for (int x = 0; x < W; ++x)
            if (map_->at(x, y) == Tile::Door && !map_->isBlocked(x, y) && !vis(x, y)) return false;

    for (const auto& p : items_) {                // tiap kasur / resepsi punya tile berdiri
        const FurnitureDef* d = findFurniture(p.defId);
        if (!d || d->type == FurnType::Decor) continue;
        IVec2 s;
        if (!standTile(p, s)) return false;
    }
    return true;
}

bool FurnitureManager::canPlace(int defId, int x, int y, int rot, IVec2 playerTile, std::string* reason) {
    auto fail = [&](const char* r) { if (reason) *reason = r; return false; };
    const FurnitureDef* d = findFurniture(defId);
    if (!d || !map_) return fail("FURNITUR TIDAK ADA");

    int w, h;
    footprintSize(*d, rot, w, h);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int tx = x + i, ty = y + j;
            if (map_->at(tx, ty) != Tile::Floor) return fail("HARUS DI ATAS LANTAI");
            if (map_->isBlocked(tx, ty)) return fail("TEMPAT SUDAH TERISI");
            if (playerTile.x == tx && playerTile.y == ty) return fail("ADA KARAKTER DI SINI");
        }

    int region = map_->regionAt(x, y);
    int lobby = map_->lobbyRegion();
    if (d->type == FurnType::Reception && region != lobby) return fail("RESEPSI HARUS DI LOBI");
    if (d->type == FurnType::Bed && region == lobby) return fail("KASUR HANYA DI KAMAR");

    // Uji coba: pasang sementara, cek jalan masih tersambung.
    PlacedFurniture trial{-1, defId, x, y, rot & 3};
    items_.push_back(trial);
    setBlockedFor(trial, true);
    bool ok = connectivityOK();
    setBlockedFor(trial, false);
    items_.pop_back();
    if (!ok) return fail("MENGHALANGI JALAN");
    return true;
}

int FurnitureManager::place(int defId, int x, int y, int rot) {
    PlacedFurniture p{nextUid_++, defId, x, y, rot & 3};
    items_.push_back(p);
    setBlockedFor(p, true);
    return p.uid;
}

bool FurnitureManager::remove(int uid) {
    auto it = std::find_if(items_.begin(), items_.end(), [&](const PlacedFurniture& p) { return p.uid == uid; });
    if (it == items_.end()) return false;
    setBlockedFor(*it, false);
    items_.erase(it);
    return true;
}

const PlacedFurniture* FurnitureManager::byUid(int uid) const {
    for (const auto& p : items_) if (p.uid == uid) return &p;
    return nullptr;
}

const PlacedFurniture* FurnitureManager::at(int tx, int ty) const {
    for (const auto& p : items_) {
        const FurnitureDef* d = findFurniture(p.defId);
        if (!d) continue;
        int w, h;
        footprintSize(*d, p.rot, w, h);
        if (tx >= p.x && tx < p.x + w && ty >= p.y && ty < p.y + h) return &p;
    }
    return nullptr;
}

Vec3 FurnitureManager::centerOf(const PlacedFurniture& p) const {
    const FurnitureDef* d = findFurniture(p.defId);
    int w = 1, h = 1;
    if (d) footprintSize(*d, p.rot, w, h);
    float ts = map_->tileSize();
    return {(p.x + w * 0.5f) * ts, 0.0f, (p.y + h * 0.5f) * ts};
}

std::vector<IVec2> FurnitureManager::frontTiles(const PlacedFurniture& p) const {
    std::vector<IVec2> out;
    const FurnitureDef* d = findFurniture(p.defId);
    if (!d) return out;
    int w, h;
    footprintSize(*d, p.rot, w, h);
    IVec2 f = facing(p.rot);
    if (f.y == 1)  for (int i = 0; i < w; ++i) out.push_back({p.x + i, p.y + h});
    if (f.y == -1) for (int i = 0; i < w; ++i) out.push_back({p.x + i, p.y - 1});
    if (f.x == 1)  for (int j = 0; j < h; ++j) out.push_back({p.x + w, p.y + j});
    if (f.x == -1) for (int j = 0; j < h; ++j) out.push_back({p.x - 1, p.y + j});
    return out;
}

bool FurnitureManager::standTile(const PlacedFurniture& p, IVec2& out) const {
    const FurnitureDef* d = findFurniture(p.defId);
    if (!d) return false;
    int w, h;
    footprintSize(*d, p.rot, w, h);

    std::vector<IVec2> cand = frontTiles(p);
    for (int i = 0; i < w; ++i) { cand.push_back({p.x + i, p.y - 1}); cand.push_back({p.x + i, p.y + h}); }
    for (int j = 0; j < h; ++j) { cand.push_back({p.x - 1, p.y + j}); cand.push_back({p.x + w, p.y + j}); }

    std::vector<uint8_t> seen;
    if (map_->hasEntrance()) seen = Pathfinder::reachable(*map_, map_->entrance());
    for (IVec2 c : cand) {
        if (!map_->isWalkable(c.x, c.y)) continue;
        if (map_->at(c.x, c.y) != Tile::Floor) continue;                 // jangan berdiri di ambang pintu
        if (map_->regionAt(c.x, c.y) != map_->regionAt(p.x, p.y)) continue;
        if (!seen.empty() && !seen[c.y * map_->width() + c.x]) continue;
        out = c;
        return true;
    }
    return false;
}

int FurnitureManager::countDecorInRegion(int region) const {
    int n = 0;
    for (const auto& p : items_) {
        const FurnitureDef* d = findFurniture(p.defId);
        if (d && d->type == FurnType::Decor && regionOf(p) == region) ++n;
    }
    return n;
}

bool FurnitureManager::hasType(FurnType t) const {
    for (const auto& p : items_) {
        const FurnitureDef* d = findFurniture(p.defId);
        if (d && d->type == t) return true;
    }
    return false;
}

std::string FurnitureManager::serialize() const {
    std::ostringstream os;
    for (const auto& p : items_) os << "F " << p.defId << ' ' << p.x << ' ' << p.y << ' ' << p.rot << '\n';
    return os.str();
}

void FurnitureManager::loadLine(const std::string& line) {
    std::istringstream is(line);
    std::string tag;
    int id, x, y, rot;
    if (!(is >> tag >> id >> x >> y >> rot) || tag != "F") return;
    const FurnitureDef* d = findFurniture(id);
    if (!d) return;
    int w, h;
    footprintSize(*d, rot, w, h);
    for (int j = 0; j < h; ++j)                      // abaikan data rusak / bertabrakan
        for (int i = 0; i < w; ++i)
            if (map_->at(x + i, y + j) != Tile::Floor || map_->isBlocked(x + i, y + j)) return;
    place(id, x, y, rot);
}

} // namespace hotel
