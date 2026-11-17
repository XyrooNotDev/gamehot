#include "game/CameraController.h"
#include <algorithm>

namespace hotel {

void CameraController::orbit(float dYaw, float dPitch) {
    yaw_ += dYaw;
    pitch_ = std::clamp(pitch_ + dPitch, 0.55f, 1.35f);
}

void CameraController::follow(Vec3 target, float dt) {
    float k = 1.0f - std::exp(-10.0f * dt);
    target_ = target_ + (target - target_) * k;
}

Vec3 CameraController::eye() const {
    float cp = std::cos(pitch_);
    return target_ + Vec3{std::sin(yaw_) * cp, std::sin(pitch_), std::cos(yaw_) * cp} * distance_;
}

} // namespace hotel
