#pragma once

#include "Vector3.h"

namespace Revora {

/// 4x4 行列 (行優先格納: m[row][col])
/// メモリレイアウトは HLSL の row_major float4x4 と一致する。
/// ベクトルは行ベクトルとして左から掛ける: v' = v * M
struct Matrix4x4 {
    float m[4][4] = {};

    Matrix4x4() = default;

    static Matrix4x4 Identity();
    static Matrix4x4 Translation(float x, float y, float z);
    static Matrix4x4 Translation(const Vector3& v);
    static Matrix4x4 Scaling(float x, float y, float z);
    static Matrix4x4 RotationX(float radians);
    static Matrix4x4 RotationY(float radians);
    static Matrix4x4 RotationZ(float radians);

    /// 左手座標系 LookAt
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up);

    /// 左手座標系 透視投影 (fovY はラジアン)
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ);

    Matrix4x4 operator*(const Matrix4x4& rhs) const;
    Matrix4x4& operator*=(const Matrix4x4& rhs);

    Matrix4x4 Transposed() const;
};

} // namespace Revora
