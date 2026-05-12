#include "Camera.h"
#include "../Math/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Revora {

void Camera::SetRotation(float pitch, float yaw) {
    // ピッチを制限して真上/真下を超えないようにする
    static constexpr float kPitchLimit = kHalfPi - 0.01f;
    pitch_ = std::max(-kPitchLimit, std::min(kPitchLimit, pitch));
    yaw_   = yaw;
}

Vector3 Camera::GetForward() const {
    // 左手座標系: +Z が前方
    float cp = std::cos(pitch_);
    return Vector3(
        std::sin(yaw_) * cp,
        std::sin(pitch_),
        std::cos(yaw_) * cp
    ).Normalized();
}

Vector3 Camera::GetRight() const {
    // Forward × Up の外積から右方向を導出
    Vector3 forward = GetForward();
    return Vector3::Cross(Vector3::Up, forward).Normalized();
}

Matrix4x4 Camera::GetViewMatrix() const {
    Vector3 target = position_ + GetForward();
    return Matrix4x4::LookAtLH(position_, target, Vector3::Up);
}

Matrix4x4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return Matrix4x4::PerspectiveFovLH(fovY_, aspectRatio, nearZ_, farZ_);
}

} // namespace Revora
