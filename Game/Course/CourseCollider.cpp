#include "CourseCollider.h"
#include "../../Engine/Physics/RigidBody.h"
#include "../../Engine/Math/MathConstants.h"

#include <cmath>

namespace Revora {

void CourseCollider::Initialize(const CatmullRomSpline* spline, float trackWidth)
{
    spline_     = spline;
    trackWidth_ = trackWidth;
}

void CourseCollider::Constrain(RigidBody& body) const
{
    if (!spline_ || spline_->GetSegmentCount() == 0) {
        return;
    }

    // スプライン上の最近点を探索
    float t = spline_->FindClosestParameter(body.position);
    Vector3 centerPos = spline_->Evaluate(t);
    Vector3 right     = spline_->EvaluateNormal(t);

    // 中心線から車両への XZ 平面上の変位ベクトル
    Vector3 toVehicle = body.position - centerPos;
    float lateralOffset = toVehicle.x * right.x + toVehicle.z * right.z;

    // トラック幅の内側にいる場合は何もしない
    if (std::abs(lateralOffset) <= trackWidth_) {
        return;
    }

    // 壁に衝突: 車両をトラック幅の境界上に押し戻す
    float sign = (lateralOffset > 0.0f) ? 1.0f : -1.0f;
    float penetration = std::abs(lateralOffset) - trackWidth_;

    // 壁法線 (コース内側を向く)
    Vector3 wallNormal = right * (-sign);

    // 位置補正: 壁法線方向に貫通量だけ押し戻す
    body.position.x += wallNormal.x * penetration;
    body.position.z += wallNormal.z * penetration;

    // 速度反射: 壁法線方向の速度成分を反転・減衰させる
    float velDotNormal = body.velocity.x * wallNormal.x
                       + body.velocity.z * wallNormal.z;

    // 壁に向かって移動している場合のみ反射
    if (velDotNormal < 0.0f) {
        body.velocity.x -= wallNormal.x * velDotNormal * (1.0f + kWallRestitution);
        body.velocity.z -= wallNormal.z * velDotNormal * (1.0f + kWallRestitution);
    }
}

} // namespace Revora
