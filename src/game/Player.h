#pragma once
#include "core/MeshData.h"
#include "world/HotelMap.h"

namespace hotel {

class Player {
public:
    Vec3 position;
    float heading = 0.0f;     // radian; arah hadap lokal = +Z
    float radius = 0.3f;
    float speed = 3.2f;       // meter/detik pada joystick penuh

    // axis: output joystick; camYaw: yaw kamera (gerak relatif terhadap kamera)
    void update(Vec2 axis, float camYaw, float dt, const HotelMap& map);
    Mat4 modelMatrix() const;
    static MeshData buildMesh(Color shirt = Color{0.20f, 0.35f, 0.75f, 1});

private:
    void resolveCollisions(const HotelMap& map);
};

} // namespace hotel
