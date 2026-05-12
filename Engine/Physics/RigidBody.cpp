#include "RigidBody.h"
#include "../Math/MathConstants.h"

#include <cmath>

namespace Revora {

void RigidBody::SetMass(float m)
{
    mass    = m;
    invMass = (m > kEpsilon) ? 1.0f / m : 0.0f;

    // 簡易慣性テンソル: 直方体近似 (幅2m × 高さ1m × 長さ4m)
    // I = (1/12) * m * (h^2 + d^2) 等の公式から対角成分を設定
    float ix = (1.0f / 12.0f) * m * (1.0f + 16.0f);  // pitch 軸
    float iy = (1.0f / 12.0f) * m * (4.0f + 16.0f);  // yaw 軸
    float iz = (1.0f / 12.0f) * m * (4.0f + 1.0f);   // roll 軸
    invInertia = {
        (ix > kEpsilon) ? 1.0f / ix : 0.0f,
        (iy > kEpsilon) ? 1.0f / iy : 0.0f,
        (iz > kEpsilon) ? 1.0f / iz : 0.0f
    };
}

void RigidBody::ApplyForce(const Vector3& force)
{
    forceAccumulator += force;
}

void RigidBody::ApplyForceAtPoint(const Vector3& force, const Vector3& worldPoint)
{
    forceAccumulator += force;

    // 接触点と重心のオフセットから生じるトルク
    Vector3 r = worldPoint - position;
    torqueAccumulator += Vector3::Cross(r, force);
}

Vector3 RigidBody::GetForward() const
{
    return rotation.RotateVector(Vector3::Forward);
}

Vector3 RigidBody::GetUp() const
{
    return rotation.RotateVector(Vector3::Up);
}

Vector3 RigidBody::GetRight() const
{
    return rotation.RotateVector(Vector3::Right);
}

Vector3 RigidBody::LocalToWorld(const Vector3& localPoint) const
{
    return position + rotation.RotateVector(localPoint);
}

Vector3 RigidBody::GetPointVelocity(const Vector3& worldPoint) const
{
    Vector3 r = worldPoint - position;
    return velocity + Vector3::Cross(angularVelocity, r);
}

void RigidBody::Integrate(float dt)
{
    // --- 半暗黙的オイラー: 速度を先に更新し、新しい速度で位置を更新 ---

    // 並進: a = F / m
    Vector3 linearAccel = forceAccumulator * invMass;
    velocity += linearAccel * dt;

    // 回転: α = I^-1 * τ (対角慣性テンソルなのでコンポーネントごとの乗算)
    Vector3 angularAccel(
        torqueAccumulator.x * invInertia.x,
        torqueAccumulator.y * invInertia.y,
        torqueAccumulator.z * invInertia.z
    );
    angularVelocity += angularAccel * dt;

    // 位置の更新 (新しい速度を使用)
    position += velocity * dt;

    // 回転の更新: q' = q + 0.5 * dt * ω * q
    // ω をクォータニオンに変換して合成
    if (angularVelocity.LengthSquared() > kEpsilon * kEpsilon) {
        Quaternion spin(
            angularVelocity.x * 0.5f * dt,
            angularVelocity.y * 0.5f * dt,
            angularVelocity.z * 0.5f * dt,
            0.0f
        );
        Quaternion dq = spin * rotation;
        rotation.x += dq.x;
        rotation.y += dq.y;
        rotation.z += dq.z;
        rotation.w += dq.w;
        rotation = rotation.Normalized();
    }

    // 蓄積された力とトルクをリセット (次フレームで再計算される)
    forceAccumulator  = Vector3::Zero;
    torqueAccumulator = Vector3::Zero;
}

} // namespace Revora
