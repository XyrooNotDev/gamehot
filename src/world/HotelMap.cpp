#include "world/HotelMap.h"
#include <algorithm>

namespace hotel {

HotelMap HotelMap::fromAscii(const std::vector<std::string>& rows, float tileSize) {
    HotelMap m;
    m.tileSize_ = tileSize;
    m.height_ = static_cast<int>(rows.size());
    for (const auto& r : rows) m.width_ = std::max(m.width_, static_cast<int>(r.size()));
    m.tiles_.assign(static_cast<size_t>(m.width_) * m.height_, Tile::Empty);
    m.blocked_.assign(m.tiles_.size(), 0);

    bool hasSpawn = false;
    for (int y = 0; y < m.height_; ++y) {
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) {
            Tile t = Tile::Empty;
            switch (rows[y][x]) {
                case '#': t = Tile::Wall; break;
                case '.': t = Tile::Floor; break;
                case 'D': t = Tile::Door; break;
                case 'E': t = Tile::Door; m.entrance_ = {x, y}; m.hasEntrance_ = true; break;
                case 'P': t = Tile::Floor; m.spawn_ = m.tileCenter(x, y); hasSpawn = true; break;
                default: break;
            }
            m.tiles_[m.idx(x, y)] = t;
        }
    }
    if (!hasSpawn) {
        for (int y = 0; y < m.height_ && !hasSpawn; ++y)
            for (int x = 0; x < m.width_ && !hasSpawn; ++x)
                if (m.at(x, y) == Tile::Floor) { m.spawn_ = m.tileCenter(x, y); hasSpawn = true; }
    }
    m.computeRegions();
    return m;
}

void HotelMap::computeRegions() {
    region_.assign(tiles_.size(), -1);
    regionCount_ = 0;
    std::vector<IVec2> stack;
    // Lobi (region tempat spawn) selalu mendapat nomor 0.
    auto fill = [&](int sx, int sy) {
        if (at(sx, sy) != Tile::Floor || region_[idx(sx, sy)] != -1) return;
        int id = regionCount_++;
        stack.push_back({sx, sy});
        region_[idx(sx, sy)] = id;
        while (!stack.empty()) {
            IVec2 c = stack.back(); stack.pop_back();
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int i = 0; i < 4; ++i) {
                int nx = c.x + dx[i], ny = c.y + dy[i];
                if (at(nx, ny) == Tile::Floor && region_[idx(nx, ny)] == -1) {
                    region_[idx(nx, ny)] = id;
                    stack.push_back({nx, ny});
                }
            }
        }
    };
    IVec2 sp = tileAt(spawn_);
    fill(sp.x, sp.y);
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) fill(x, y);
}

} // namespace hotel
