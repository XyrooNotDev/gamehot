#include "world/Pathfinder.h"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace hotel {

namespace {
const int kDx[4] = {1, -1, 0, 0};
const int kDy[4] = {0, 0, 1, -1};
}

bool Pathfinder::findPath(const HotelMap& map, IVec2 start, IVec2 goal, std::vector<IVec2>& out) {
    out.clear();
    if (start == goal) return true;
    if (!map.isWalkable(goal.x, goal.y)) return false;

    const int W = map.width(), H = map.height();
    auto id = [&](int x, int y) { return y * W + x; };
    if (start.x < 0 || start.y < 0 || start.x >= W || start.y >= H) return false;

    std::vector<int> g(static_cast<size_t>(W) * H, 1 << 28), parent(g.size(), -1);
    using Node = std::pair<int, int>;  // (f, id)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    g[id(start.x, start.y)] = 0;
    open.push({std::abs(goal.x - start.x) + std::abs(goal.y - start.y), id(start.x, start.y)});

    while (!open.empty()) {
        auto [f, cur] = open.top();
        open.pop();
        int cx = cur % W, cy = cur / W;
        if (cx == goal.x && cy == goal.y) break;
        if (f > g[cur] + std::abs(goal.x - cx) + std::abs(goal.y - cy)) continue;  // entri usang
        for (int i = 0; i < 4; ++i) {
            int nx = cx + kDx[i], ny = cy + kDy[i];
            if (!map.isWalkable(nx, ny)) continue;
            int ng = g[cur] + 1, nid = id(nx, ny);
            if (ng < g[nid]) {
                g[nid] = ng;
                parent[nid] = cur;
                open.push({ng + std::abs(goal.x - nx) + std::abs(goal.y - ny), nid});
            }
        }
    }
    int gid = id(goal.x, goal.y);
    if (parent[gid] < 0) return false;
    for (int c = gid; c != id(start.x, start.y); c = parent[c]) out.push_back({c % W, c / W});
    std::reverse(out.begin(), out.end());
    return true;
}

std::vector<uint8_t> Pathfinder::reachable(const HotelMap& map, IVec2 start) {
    const int W = map.width(), H = map.height();
    std::vector<uint8_t> seen(static_cast<size_t>(W) * H, 0);
    if (!map.isWalkable(start.x, start.y)) return seen;
    std::vector<IVec2> stack{start};
    seen[start.y * W + start.x] = 1;
    while (!stack.empty()) {
        IVec2 c = stack.back(); stack.pop_back();
        for (int i = 0; i < 4; ++i) {
            int nx = c.x + kDx[i], ny = c.y + kDy[i];
            if (map.isWalkable(nx, ny) && !seen[ny * W + nx]) {
                seen[ny * W + nx] = 1;
                stack.push_back({nx, ny});
            }
        }
    }
    return seen;
}

} // namespace hotel
