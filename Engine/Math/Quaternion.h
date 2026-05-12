#pragma once

#include "Vector3.h"
#include "Matrix4x4.h"

namespace Revora {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quaternion() = default;
    Quaternion(float x, float y, float z, float w);

    /// 任意軸回転 (axisは正規化済みであること, angleはラジアン)
    static Quaternion FromAxisAngle(const Vector3& axis, float angle);

    /// オイラー角から生成 (各軸ラジアン, 適用順序: Y→X→Z)
    static Quaternion FromEuler(float pitch, float yaw, float roll);

    /// 行優先の回転行列に変換
    Matrix4x4 ToMatrix() const;

    /// 球面線形補間
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

    Quaternion operator*(const Quaternion& rhs) const;

    /// クォータニオンでベクトルを回転させる (ローカル→ワールド方向変換)
    /// q * v * q^-1 の最適化形式
    Vector3 RotateVector(const Vector3& v) const;

    float     Length() const;
    Quaternion Normalized() const;

    static Quaternion Conjugate(const Quaternion& q);

    static const Quaternion Identity;
};

} // namespace Revora
