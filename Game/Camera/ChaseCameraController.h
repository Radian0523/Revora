#pragma once

#include "../../Engine/Renderer/Camera.h"
#include "../../Engine/Math/Vector3.h"
#include "../../Engine/Math/Quaternion.h"

namespace Revora {

/// 車両追従カメラコントローラー
/// 指数平滑で車両後方を滑らかに追従し、速度に連動して FOV を変化させる
/// Camera の position/rotation を外部から操作する設計のため Game 層に配置
class ChaseCameraController {
public:
    void Initialize(Camera& camera);

    /// 車両の位置・回転・速度に基づいてカメラを更新する
    void Update(const Vector3& targetPosition,
                const Quaternion& targetRotation,
                float speed,
                float dt);

    void SetFollowDistance(float dist) { followDistance_ = dist; }
    void SetFollowHeight(float h)     { followHeight_ = h; }
    void SetLookAheadDistance(float d) { lookAheadDistance_ = d; }
    void SetSmoothSpeed(float s)      { smoothSpeed_ = s; }

private:
    Camera* camera_ = nullptr;

    // カメラ配置パラメータ
    float followDistance_   = 6.0f;   // 車両後方の距離
    float followHeight_    = 2.5f;    // 車両上方の高さ
    float lookAheadDistance_ = 3.0f;  // 車両前方を見る距離

    // スムージング
    float smoothSpeed_     = 5.0f;    // 指数平滑の速度係数

    // 速度連動 FOV
    float baseFovY_        = 0.7854f; // 45度 (静止時)
    float maxFovY_         = 1.0472f; // 60度 (最高速時)
    float fovSpeedRef_     = 40.0f;   // この速度で maxFovY に到達

    // 前フレームのカメラ位置 (スムージング用)
    Vector3 currentCameraPos_;
    bool    isFirstFrame_ = true;
};

} // namespace Revora
