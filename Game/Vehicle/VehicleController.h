#pragma once

#include "../../Engine/Physics/VehiclePhysics.h"
#include "../../Engine/Physics/VehicleConfig.h"

namespace Revora {

class InputManager;

/// 車両操作の Game 層ラッパー
/// 入力のスムージングとステアリング補間を行い、VehiclePhysics に受け渡す
/// リセット処理やゲーム固有の操作ロジックもここに集約する
class VehicleController {
public:
    bool Initialize(const VehicleConfig& config);

    /// 固定タイムステップで呼ばれる
    void Update(const InputManager& input, float dt);

    /// 車両をスポーン位置にリセットする
    void Reset();

    VehiclePhysics&       GetPhysics()       { return physics_; }
    const VehiclePhysics& GetPhysics() const { return physics_; }

private:
    /// ステアリング入力を指数平滑でスムージングする
    /// キーボードのデジタル入力を滑らかなアナログ値に変換する
    float SmoothSteering(float target, float current, float dt) const;

    VehiclePhysics physics_;
    VehicleConfig  config_;

    float currentSteering_ = 0.0f;  // スムージング済みステアリング値 (-1~1)
};

} // namespace Revora
