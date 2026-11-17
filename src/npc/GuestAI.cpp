#include "npc/GuestAI.h"
#include <algorithm>
#include <queue>
#include "world/Pathfinder.h"

namespace hotel {

void GuestManager::init(HotelMap* map, FurnitureManager* furn, Economy* eco, unsigned seed) {
    map_ = map; furn_ = furn; eco_ = eco; rng_ = seed ? seed : 1;
    reset();
}

void GuestManager::reset() {
    guests_.clear();
    events_.clear();
    spawnTimer_ = 4.0f;
    nextId_ = 1;
}

float GuestManager::rnd(float a, float b) {
    rng_ = rng_ * 1664525u + 1013904223u;
    return a + (b - a) * ((rng_ >> 8) & 0xFFFF) / 65535.0f;
}

std::vector<IVec2> GuestManager::queueSlots() const {
    std::vector<IVec2> slots;
    for (const auto& p : furn_->items()) {
        const FurnitureDef* d = findFurniture(p.defId);
        if (!d || d->type != FurnType::Reception) continue;
        // BFS dari tile depan meja -> slot antrean berurutan dari yang terdekat
        std::vector<IVec2> frontier = furn_->frontTiles(p);
        std::vector<uint8_t> seen(static_cast<size_t>(map_->width()) * map_->height(), 0);
        std::queue<IVec2> q;
        for (IVec2 f : frontier)
            if (map_->isWalkable(f.x, f.y)) { q.push(f); seen[f.y * map_->width() + f.x] = 1; }
        while (!q.empty() && static_cast<int>(slots.size()) < kQueueSlots) {
            IVec2 c = q.front(); q.pop();
            if (map_->at(c.x, c.y) == Tile::Floor &&
                std::find(slots.begin(), slots.end(), c) == slots.end()) slots.push_back(c);
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int i = 0; i < 4; ++i) {
                int nx = c.x + dx[i], ny = c.y + dy[i];
                if (map_->isWalkable(nx, ny) && !seen[ny * map_->width() + nx]) {
                    seen[ny * map_->width() + nx] = 1;
                    q.push({nx, ny});
                }
            }
        }
        break;  // satu antrean (resepsionis pertama) cukup untuk saat ini
    }
    return slots;
}

bool GuestManager::nearReception(Vec3 p) const {
    for (const auto& r : furn_->items()) {
        const FurnitureDef* d = findFurniture(r.defId);
        if (!d || d->type != FurnType::Reception) continue;
        Vec3 c = furn_->centerOf(r);
        float dx = c.x - p.x, dz = c.z - p.z;
        if (dx * dx + dz * dz <= 3.2f * 3.2f * map_->tileSize() * map_->tileSize()) return true;
    }
    return false;
}

bool GuestManager::setPath(Guest& g, IVec2 goal) {
    g.target = goal;
    g.pathIdx = 0;
    return Pathfinder::findPath(*map_, map_->tileAt(g.pos), goal, g.path);
}

int GuestManager::fareFor(const PlacedFurniture& bed) const {
    const FurnitureDef* d = findFurniture(bed.defId);
    int base = d ? d->rate : 0;
    int roomDecor = std::min(5, furn_->countDecorInRegion(furn_->regionOf(bed)));
    int lobbyDecor = std::min(10, furn_->countDecorInRegion(map_->lobbyRegion()));
    return base + roomDecor * 8 + lobbyDecor * 2;
}

bool GuestManager::canCheckIn(Vec3 playerPos) const {
    if (!nearReception(playerPos)) return false;
    for (const auto& g : guests_) if (g.state == GuestState::Waiting) return true;
    return false;
}

bool GuestManager::checkIn(Vec3 playerPos, std::string& msg) {
    if (!nearReception(playerPos)) { msg = "DEKATI RESEPSI"; return false; }
    Guest* guest = nullptr;
    for (auto& g : guests_)
        if (g.state == GuestState::Waiting && (!guest || g.patience < guest->patience)) guest = &g;
    if (!guest) { msg = "TIDAK ADA TAMU"; return false; }

    // Kasur bebas terbaik (tarif tertinggi) yang bisa dijangkau.
    const PlacedFurniture* best = nullptr;
    IVec2 bestStand{};
    int bestFare = -1;
    for (const auto& b : furn_->items()) {
        const FurnitureDef* d = findFurniture(b.defId);
        if (!d || d->type != FurnType::Bed) continue;
        bool taken = false;
        for (const auto& g : guests_)
            if (g.bedUid == b.uid && (g.state == GuestState::ToRoom || g.state == GuestState::Staying)) taken = true;
        if (taken) continue;
        IVec2 stand;
        if (!furn_->standTile(b, stand)) continue;
        int fare = fareFor(b);
        if (fare > bestFare) { best = &b; bestStand = stand; bestFare = fare; }
    }
    if (!best) {
        msg = "KAMAR PENUH";
        events_.push_back({GameEvent::NoRoom, 0, guest->pos});
        return false;
    }
    guest->bedUid = best->uid;
    guest->slot = -1;
    if (!setPath(*guest, bestStand)) { guest->bedUid = -1; msg = "TIDAK ADA JALAN"; return false; }
    guest->state = GuestState::ToRoom;
    msg = "CHECK-IN BERHASIL";
    events_.push_back({GameEvent::CheckedIn, 0, guest->pos});
    return true;
}

void GuestManager::beginLeaving(Guest& g, bool pay) {
    g.state = GuestState::Leaving;
    g.paysOnExit = false;
    if (pay && g.fare > 0) {
        eco_->earn(g.fare);
        eco_->served++;
        events_.push_back({GameEvent::Paid, g.fare, g.pos});
    }
    g.slot = -1;
    if (!map_->hasEntrance() || !setPath(g, map_->entrance())) g.state = GuestState::Gone;
}

void GuestManager::follow(Guest& g, float dt) {
    const float speed = 1.9f * map_->tileSize();
    float remaining = speed * dt;
    while (remaining > 0 && g.pathIdx < g.path.size()) {
        IVec2 t = g.path[g.pathIdx];
        if (map_->isSolid(t.x, t.y)) {              // furnitur baru menghalangi -> cari jalan lain
            if (!setPath(g, g.target)) return;
            if (g.path.empty()) return;
            continue;
        }
        Vec3 c = map_->tileCenter(t.x, t.y);
        float dx = c.x - g.pos.x, dz = c.z - g.pos.z;
        float d = std::sqrt(dx * dx + dz * dz);
        if (d > 1e-4f) g.heading = std::atan2(dx, dz);
        if (d <= remaining) { g.pos = c; remaining -= d; ++g.pathIdx; }
        else { g.pos.x += dx / d * remaining; g.pos.z += dz / d * remaining; remaining = 0; }
    }
}

void GuestManager::update(float dt) {
    // ---- spawn ----
    spawnTimer_ -= dt;
    if (spawnTimer_ <= 0) {
        spawnTimer_ = rnd(10.0f, 18.0f);
        auto slots = queueSlots();
        int active = 0;
        for (const auto& g : guests_) if (g.state != GuestState::Gone) ++active;
        if (map_->hasEntrance() && !slots.empty() && active < kMaxGuests) {
            for (int s = 0; s < static_cast<int>(slots.size()); ++s) {
                bool used = false;
                for (const auto& g : guests_)
                    if (g.slot == s && (g.state == GuestState::Arriving || g.state == GuestState::Waiting)) used = true;
                if (used) continue;
                Guest g;
                g.id = nextId_++;
                g.pos = map_->tileCenter(map_->entrance().x, map_->entrance().y);
                g.variant = static_cast<int>(rnd(0, 3.99f));
                g.slot = s;
                g.patienceMax = g.patience = rnd(40.0f, 55.0f);
                if (setPath(g, slots[s])) {
                    guests_.push_back(g);
                    events_.push_back({GameEvent::GuestArrived, 0, g.pos});
                }
                break;
            }
        }
    }

    // ---- update tiap tamu ----
    for (auto& g : guests_) {
        switch (g.state) {
        case GuestState::Arriving:
            follow(g, dt);
            if (g.pathIdx >= g.path.size()) g.state = GuestState::Waiting;
            break;
        case GuestState::Waiting:
            g.patience -= dt;
            if (g.patience <= 0) {
                events_.push_back({GameEvent::GuestLeftAngry, 0, g.pos});
                beginLeaving(g, false);
            }
            break;
        case GuestState::ToRoom:
            if (!furn_->byUid(g.bedUid)) { beginLeaving(g, false); break; }   // kasur dijual
            follow(g, dt);
            if (g.pathIdx >= g.path.size()) {
                g.state = GuestState::Staying;
                g.timer = rnd(18.0f, 28.0f);
                g.fare = fareFor(*furn_->byUid(g.bedUid));
                Vec3 c = furn_->centerOf(*furn_->byUid(g.bedUid));
                g.pos = c;
            }
            break;
        case GuestState::Staying: {
            const PlacedFurniture* bed = furn_->byUid(g.bedUid);
            if (!bed) {                                       // kasur dijual saat tamu tidur
                IVec2 e = map_->hasEntrance() ? map_->entrance() : IVec2{0, 0};
                g.pos = map_->tileCenter(e.x, e.y);
                g.state = GuestState::Gone;
                break;
            }
            g.timer -= dt;
            if (g.timer <= 0) {
                IVec2 stand;
                if (furn_->standTile(*bed, stand)) g.pos = map_->tileCenter(stand.x, stand.y);
                g.bedUid = -1;
                beginLeaving(g, true);
            }
            break;
        }
        case GuestState::Leaving:
            follow(g, dt);
            if (g.pathIdx >= g.path.size()) g.state = GuestState::Gone;
            break;
        case GuestState::Gone:
            break;
        }
    }
    guests_.erase(std::remove_if(guests_.begin(), guests_.end(),
                  [](const Guest& g) { return g.state == GuestState::Gone; }), guests_.end());
}

std::vector<GameEvent> GuestManager::takeEvents() {
    std::vector<GameEvent> out;
    out.swap(events_);
    return out;
}

} // namespace hotel
