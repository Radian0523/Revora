#pragma once

#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"

namespace Revora {

/// 剛体の物理状態と力の蓄積・積分を管理する構造体
/// VehiclePhysics から分離することで、衝突リゾルバが車両固有の詳細を
/// 知らずに位置・速度を修正できる設計になっている
struct RigidBody {
    // --- 状態 ---
    Vector3    position;
    Quaternion rotation = Quaternion::Identity;
    Vector3    velocity;
    Vector3    angularVelocity;

    // --- 質量プロパティ ---
    float mass    = 1200.0f;
    float invMass = 1.0f / 1200.0f;

    // 慣性モーメントの逆数 (簡易的に対角成分のみ)
    Vector3 invInertia = {1.0f / 800.0f, 1.0f / 1200.0f, 1.0f / 800.0f};

    // --- 蓄積された力・トルク (Integrate() で消費される) ---
    Vector3 forceAccumulator;
    Vector3 torqueAccumulator;

    void SetMass(float m);

    /// 重心に力を加える (並進のみ)
    void ApplyForce(const Vector3& force);

    /// ワールド空間の指定点に力を加える (並進 + 回転)
    /// 接触点がずれるとトルクが発生する
    void ApplyForceAtPoint(const Vector3& force, const Vector3& worldPoint);

    /// ローカル方向をワールド方向に変換する
    Vector3 GetForward() const;
    Vector3 GetUp()      const;
    Vector3 GetRight()   const;

    /// ローカル座標をワールド座標に変換する
    Vector3 LocalToWorld(const Vector3& localPoint) const;

    /// ワールド空間の指定点における速度を取得する
    /// 剛体の並進速度 + 角速度による回転速度を合成
    Vector3 GetPointVelocity(const Vector3& worldPoint) const;

    /// 半暗黙的オイラー積分
    /// 速度を先に更新してから位置に反映するため、
    /// サスペンションのような振動系でエネルギーが保存されやすい
    void Integrate(float dt);
};

} // namespace Revora
