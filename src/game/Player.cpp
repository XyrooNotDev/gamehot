#include "game/Player.h"
#include <algorithm>

namespace hotel {

void Player::update(Vec2 axis, float camYaw, float dt, const HotelMap& map) {
    float len = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    if (len < 0.01f) return;
    if (len > 1.0f) { axis.x /= len; axis.y /= len; len = 1.0f; }

    Vec3 fwd{-std::sin(camYaw), 0, -std::cos(camYaw)};
    Vec3 right{std::cos(camYaw), 0, -std::sin(camYaw)};
    Vec3 dir = fwd * axis.y + right * axis.x;

    position = position + dir * (speed * dt);
    resolveCollisions(map);

    // putar badan perlahan ke arah gerak (jalur terpendek)
    float target = std::atan2(dir.x, dir.z);
    float diff = target - heading;
    while (diff > kPi) diff -= 2 * kPi;
    while (diff < -kPi) diff += 2 * kPi;
    heading += diff * std::min(1.0f, 14.0f * dt);
}

void Player::resolveCollisions(const HotelMap& map) {
    const float ts = map.tileSize();
    const float r2 = radius * radius;
    for (int iter = 0; iter < 3; ++iter) {
        int cx = static_cast<int>(std::floor(position.x / ts));
        int cz = static_cast<int>(std::floor(position.z / ts));
        for (int tz = cz - 1; tz <= cz + 1; ++tz) {
            for (int tx = cx - 1; tx <= cx + 1; ++tx) {
                if (!map.isSolid(tx, tz)) continue;
                float minX = tx * ts, maxX = minX + ts, minZ = tz * ts, maxZ = minZ + ts;
                float px = std::clamp(position.x, minX, maxX);
                float pz = std::clamp(position.z, minZ, maxZ);
                float dx = position.x - px, dz = position.z - pz;
                float d2 = dx * dx + dz * dz;
                if (d2 >= r2) continue;
                if (d2 > 1e-8f) {
                    float d = std::sqrt(d2), push = (radius - d) / d;
                    position.x += dx * push;
                    position.z += dz * push;
                } else {  // titik pusat sudah di dalam tile: dorong lewat sisi terdekat
                    float l = position.x - minX, rr = maxX - position.x;
                    float t = position.z - minZ, b = maxZ - position.z;
                    float m = std::min(std::min(l, rr), std::min(t, b));
                    if (m == l) position.x = minX - radius;
                    else if (m == rr) position.x = maxX + radius;
                    else if (m == t) position.z = minZ - radius;
                    else position.z = maxZ + radius;
                }
            }
        }
    }
}

Mat4 Player::modelMatrix() const {
    return Mat4::translation(position) * Mat4::rotationY(heading);
}

// Karakter placeholder: badan + kepala + "hidung" agar arah hadap terlihat.
MeshData Player::buildMesh(Color shirt) {
    MeshData m;
    m.addBox({-0.22f, 0.0f, -0.14f}, {0.22f, 0.95f, 0.14f}, shirt);
    m.addBox({-0.16f, 0.95f, -0.16f}, {0.16f, 1.30f, 0.16f}, {0.95f, 0.78f, 0.62f, 1});
    m.addBox({-0.05f, 1.08f, 0.16f}, {0.05f, 1.16f, 0.26f}, {0.90f, 0.30f, 0.25f, 1});
    return m;
}

} // namespace hotel
