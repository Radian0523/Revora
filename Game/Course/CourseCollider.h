#pragma once

#include "../../Engine/Math/CatmullRomSpline.h"
#include "../../Engine/Math/Vector3.h"

namespace Revora {

struct RigidBody;

/// スプラインベースのコース境界拘束
/// 車両がトラック幅の外に出た場合、位置を補正し速度を壁反射させる
/// Engine 層の VehiclePhysics には依存せず、RigidBody の位置・速度のみ操作する
class CourseCollider {
public:
    /// コースのスプラインとトラック幅を設定する
    void Initialize(const CatmullRomSpline* spline, float trackWidth);

    /// 車両の位置がコース境界外であれば押し戻し、速度を壁法線で反射する
    /// VehicleController から物理更新後に呼ばれる
    void Constrain(RigidBody& body) const;

private:
    const CatmullRomSpline* spline_ = nullptr;
    float trackWidth_ = 8.0f;

    // 壁衝突時の速度反射で失われるエネルギーの割合
    // 1.0 で完全弾性反射、0.0 で完全吸収
    static constexpr float kWallRestitution = 0.5f;
};

} // namespace Revora
