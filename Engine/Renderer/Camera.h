#pragma once

#include "../Math/Matrix4x4.h"
#include "../Math/Vector3.h"

namespace Revora {

/// ビュー / 射影行列の計算を担当する純粋な数学クラス
/// 入力処理は持たず、外部のコントローラーが position / rotation を操作する
class Camera {
public:
    Camera() = default;

    void SetPosition(const Vector3& position) { position_ = position; }
    void SetRotation(float pitch, float yaw);

    const Vector3& GetPosition() const { return position_; }
    float GetPitch() const { return pitch_; }
    float GetYaw()   const { return yaw_; }

    /// 前方方向ベクトル (正規化済み)
    Vector3 GetForward() const;

    /// 右方向ベクトル (正規化済み)
    Vector3 GetRight() const;

    /// ビュー行列を計算する (LookAt)
    Matrix4x4 GetViewMatrix() const;

    /// 射影行列を計算する (Perspective)
    Matrix4x4 GetProjectionMatrix(float aspectRatio) const;

    void SetFovY(float fovY)   { fovY_ = fovY; }
    void SetNearZ(float nearZ) { nearZ_ = nearZ; }
    void SetFarZ(float farZ)   { farZ_ = farZ; }

private:
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    float   pitch_    = 0.0f;  // ラジアン (上下の回転)
    float   yaw_      = 0.0f;  // ラジアン (左右の回転)
    float   fovY_     = 0.7854f;  // 45度
    float   nearZ_    = 0.1f;
    float   farZ_     = 1000.0f;
};

} // namespace Revora
