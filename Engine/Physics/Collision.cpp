#include "Collision.h"
#include "../Math/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Revora {
namespace Collision {

HitResult RayPlane(const Vector3& rayOrigin,
                   const Vector3& rayDirection,
                   const Vector3& planePoint,
                   const Vector3& planeNormal,
                   float maxDistance)
{
    HitResult result;

    float denom = Vector3::Dot(rayDirection, planeNormal);

    // レイが平面とほぼ平行な場合は交差なし
    if (std::abs(denom) < kEpsilon) {
        return result;
    }

    // レイ原点から平面までの符号付き距離を算出
    float t = Vector3::Dot(planePoint - rayOrigin, planeNormal) / denom;

    // 交差点がレイの前方かつ最大距離以内であること
    if (t < 0.0f || t > maxDistance) {
        return result;
    }

    result.hit      = true;
    result.distance = t;
    result.point    = rayOrigin + rayDirection * t;
    result.normal   = planeNormal;

    return result;
}

Vector3 ClampToBounds(const Vector3& position, float halfExtent)
{
    return Vector3(
        std::clamp(position.x, -halfExtent, halfExtent),
        position.y,
        std::clamp(position.z, -halfExtent, halfExtent)
    );
}

} // namespace Collision
} // namespace Revora
