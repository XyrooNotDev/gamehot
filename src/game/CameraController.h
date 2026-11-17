#pragma once
#include "core/Math.h"

namespace hotel {

// Kamera orbit orang-ketiga yang mengikuti target dengan halus.
class CameraController {
public:
    void orbit(float dYaw, float dPitch);
    void follow(Vec3 target, float dt);
    void snapTo(Vec3 target) { target_ = target; }

    float yaw() const { return yaw_; }
    Vec3 eye() const;
    Mat4 view() const { return Mat4::lookAt(eye(), target_, {0, 1, 0}); }
    Mat4 projection(float aspect) const { return Mat4::perspective(fovY_, aspect, 0.1f, 100.0f); }

private:
    Vec3 target_;
    float yaw_ = 0.5f;
    float pitch_ = 1.0f;      // ~57 derajat dari horizontal
    float distance_ = 7.0f;
    float fovY_ = 50.0f * kPi / 180.0f;
};

} // namespace hotel
