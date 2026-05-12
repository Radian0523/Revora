#include "ChaseCameraController.h"
#include "../../Engine/Math/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Revora {

void ChaseCameraController::Initialize(Camera& camera)
{
    camera_       = &camera;
    isFirstFrame_ = true;
}

void ChaseCameraController::Update(const Vector3& targetPosition,
                                    const Quaternion& targetRotation,
                                    float speed,
                                    float dt)
{
    if (!camera_) {
        return;
    }

    // 車体の前方・上方向をクォータニオンから取得
    Vector3 vehicleForward = targetRotation.RotateVector(Vector3::Forward);
    Vector3 vehicleUp      = targetRotation.RotateVector(Vector3::Up);

    // 理想的なカメラ位置: 車両後方 + 上方
    Vector3 desiredPos = targetPosition
                       - vehicleForward * followDistance_
                       + Vector3::Up * followHeight_;

    // 初回フレームはスナップ、以降は指数平滑で追従
    if (isFirstFrame_) {
        currentCameraPos_ = desiredPos;
        isFirstFrame_     = false;
    }
    else {
        // 指数平滑: 大きい smoothSpeed_ ほど素早く追従
        float t = 1.0f - std::exp(-smoothSpeed_ * dt);
        currentCameraPos_ = Vector3::Lerp(currentCameraPos_, desiredPos, t);
    }

    camera_->SetPosition(currentCameraPos_);

    // 注視点: 車両の少し前方を見る
    Vector3 lookTarget = targetPosition + vehicleForward * lookAheadDistance_;

    // カメラの向きを注視点に向ける (pitch/yaw に変換)
    Vector3 toTarget = (lookTarget - currentCameraPos_).Normalized();

    float yaw   = std::atan2(toTarget.x, toTarget.z);
    float pitch = std::asin(std::clamp(toTarget.y, -1.0f, 1.0f));

    camera_->SetRotation(pitch, yaw);

    // 速度連動 FOV: 速度が上がるほど FOV を広げてスピード感を演出
    float speedRatio = std::clamp(speed / fovSpeedRef_, 0.0f, 1.0f);
    float fov = baseFovY_ + (maxFovY_ - baseFovY_) * speedRatio;
    camera_->SetFovY(fov);
}

} // namespace Revora
