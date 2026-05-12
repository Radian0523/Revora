#pragma once

#include "../Math/Vector3.h"

namespace Revora {

/// レイキャストの交差結果
struct HitResult {
    bool    hit      = false;
    float   distance = 0.0f;
    Vector3 point;
    Vector3 normal;
};

/// Phase 3 用の簡易コリジョンユーティリティ
/// Phase 4 以降で OBB・メッシュ衝突に拡張予定
namespace Collision {

/// レイと無限平面の交差判定
/// planeNormal は正規化済みであること
/// レイが平面と平行な場合は false を返す
HitResult RayPlane(const Vector3& rayOrigin,
                   const Vector3& rayDirection,
                   const Vector3& planePoint,
                   const Vector3& planeNormal,
                   float maxDistance);

/// 座標を矩形境界内にクランプする (XZ 平面上)
/// 境界外に出た場合は境界上に押し戻す
Vector3 ClampToBounds(const Vector3& position,
                      float halfExtent);

} // namespace Collision

} // namespace Revora
