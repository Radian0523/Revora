#include "Quaternion.h"
#include "MathConstants.h"
#include <cmath>
#include <cstring>

namespace Revora {

const Quaternion Quaternion::Identity = {0.0f, 0.0f, 0.0f, 1.0f};

Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w) {}

Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float angle) {
    float half = angle * 0.5f;
    float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

Quaternion Quaternion::FromEuler(float pitch, float yaw, float roll) {
    // 適用順序 Y(yaw) → X(pitch) → Z(roll)
    Quaternion qy = FromAxisAngle(Vector3::Up, yaw);
    Quaternion qx = FromAxisAngle(Vector3::Right, pitch);
    Quaternion qz = FromAxisAngle(Vector3::Forward, roll);
    return qy * qx * qz;
}

Matrix4x4 Quaternion::ToMatrix() const {
    // 行優先・行ベクトル方式の回転行列
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));

    result.m[0][0] = 1.0f - 2.0f * (yy + zz);
    result.m[0][1] = 2.0f * (xy + wz);
    result.m[0][2] = 2.0f * (xz - wy);

    result.m[1][0] = 2.0f * (xy - wz);
    result.m[1][1] = 1.0f - 2.0f * (xx + zz);
    result.m[1][2] = 2.0f * (yz + wx);

    result.m[2][0] = 2.0f * (xz + wy);
    result.m[2][1] = 2.0f * (yz - wx);
    result.m[2][2] = 1.0f - 2.0f * (xx + yy);

    result.m[3][3] = 1.0f;

    return result;
}

Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    // 最短経路を選ぶため符号を反転
    Quaternion target = b;
    if (dot < 0.0f) {
        dot = -dot;
        target.x = -target.x;
        target.y = -target.y;
        target.z = -target.z;
        target.w = -target.w;
    }

    // ほぼ同一の場合は線形補間にフォールバック
    if (dot > 0.9995f) {
        return Quaternion(
            a.x + t * (target.x - a.x),
            a.y + t * (target.y - a.y),
            a.z + t * (target.z - a.z),
            a.w + t * (target.w - a.w)
        ).Normalized();
    }

    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);
    float wa = std::sin((1.0f - t) * theta) / sinTheta;
    float wb = std::sin(t * theta) / sinTheta;

    return Quaternion(
        wa * a.x + wb * target.x,
        wa * a.y + wb * target.y,
        wa * a.z + wb * target.z,
        wa * a.w + wb * target.w
    );
}

Quaternion Quaternion::operator*(const Quaternion& rhs) const {
    return {
        w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
        w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
        w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
        w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
    };
}

float Quaternion::Length() const {
    return std::sqrt(x * x + y * y + z * z + w * w);
}

Quaternion Quaternion::Normalized() const {
    float len = Length();
    if (len < kEpsilon) {
        return Identity;
    }
    float inv = 1.0f / len;
    return {x * inv, y * inv, z * inv, w * inv};
}

Quaternion Quaternion::Conjugate(const Quaternion& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

} // namespace Revora
