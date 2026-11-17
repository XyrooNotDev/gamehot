#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/Math.h"

namespace hotel {

enum class Tile : uint8_t { Empty, Floor, Wall, Door };

// Peta grid 2D. Baris peta = sumbu Z dunia, kolom = sumbu X, Y = atas.
// Legenda ASCII: '#' dinding, '.' lantai, 'D' pintu, 'E' pintu masuk hotel (tamu muncul),
//                'P' titik spawn pemain, ' ' kosong.
// Lapisan "blocked" dipakai furnitur agar tile tidak bisa dilewati.
// Region = kumpulan lantai yang terhubung tanpa melewati pintu (lobi / tiap kamar).
class HotelMap {
public:
    static HotelMap fromAscii(const std::vector<std::string>& rows, float tileSize = 1.0f);

    int width() const { return width_; }
    int height() const { return height_; }
    float tileSize() const { return tileSize_; }

    Tile at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return Tile::Empty;
        return tiles_[idx(x, y)];
    }
    bool isBlocked(int x, int y) const {
        return x >= 0 && y >= 0 && x < width_ && y < height_ && blocked_[idx(x, y)] != 0;
    }
    void setBlocked(int x, int y, bool b) {
        if (x >= 0 && y >= 0 && x < width_ && y < height_) blocked_[idx(x, y)] = b ? 1 : 0;
    }
    bool isSolid(int x, int y) const {
        Tile t = at(x, y);
        return t == Tile::Wall || t == Tile::Empty || isBlocked(x, y);
    }
    bool isWalkable(int x, int y) const {
        Tile t = at(x, y);
        return (t == Tile::Floor || t == Tile::Door) && !isBlocked(x, y);
    }

    Vec3 tileCenter(int x, int y) const { return {(x + 0.5f) * tileSize_, 0.0f, (y + 0.5f) * tileSize_}; }
    IVec2 tileAt(Vec3 p) const {
        return {static_cast<int>(std::floor(p.x / tileSize_)), static_cast<int>(std::floor(p.z / tileSize_))};
    }

    Vec3 spawn() const { return spawn_; }
    bool hasEntrance() const { return hasEntrance_; }
    IVec2 entrance() const { return entrance_; }

    // -1 jika bukan lantai biasa (dinding, pintu, kosong).
    int regionAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) return -1;
        return region_[idx(x, y)];
    }
    int regionCount() const { return regionCount_; }
    int lobbyRegion() const { IVec2 t = tileAt(spawn_); return regionAt(t.x, t.y); }

private:
    size_t idx(int x, int y) const { return static_cast<size_t>(y) * width_ + x; }
    void computeRegions();

    int width_ = 0, height_ = 0;
    float tileSize_ = 1.0f;
    std::vector<Tile> tiles_;
    std::vector<uint8_t> blocked_;
    std::vector<int> region_;
    int regionCount_ = 0;
    Vec3 spawn_;
    bool hasEntrance_ = false;
    IVec2 entrance_;
};

} // namespace hotel
