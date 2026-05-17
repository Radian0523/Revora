#include "VehicleController.h"
#include "VehicleInputMapper.h"
#include "../../Engine/Input/InputManager.h"
#include "../../Engine/Math/MathConstants.h"

#include <SDL.h>

#include <cmath>

namespace Revora {

bool VehicleController::Initialize(const VehicleConfig& config)
{
    config_ = config;
    physics_.Initialize(config_);
    currentSteering_ = 0.0f;
    return true;
}

void VehicleController::SetCourseCollider(const CourseCollider* collider)
{
    courseCollider_ = collider;

    // コースシステム使用時は矩形境界クランプを無効化
    physics_.SetBoundaryClampEnabled(collider == nullptr);
}

void VehicleController::Update(const InputManager& input, float dt)
{
    // R キーでリセット
    if (input.IsKeyPressed(SDL_SCANCODE_R)) {
        Reset();
        return;
    }

    // 入力をマッピング
    VehicleInputState vehicleInput = VehicleInputMapper::MapInput(input);

    // ステアリングのスムージング
    currentSteering_ = SmoothSteering(vehicleInput.steering, currentSteering_, dt);

    // スムージング済みステアリングをラジアンに変換して物理に設定
    float steerAngleRad = currentSteering_ * config_.maxSteerAngle * kDegToRad;
    physics_.SetSteerAngle(steerAngleRad);

    // 物理シミュレーション更新
    physics_.Update(dt, vehicleInput);

    // コース境界拘束: 物理更新後に適用することで Engine 層への依存を回避
    lastCollision_ = {};
    if (courseCollider_) {
        lastCollision_ = courseCollider_->Constrain(physics_.GetBody());
    }
}

void VehicleController::Reset()
{
    physics_.Reset();
    currentSteering_ = 0.0f;
}

float VehicleController::SmoothSteering(float target, float current, float dt) const
{
    // ステアリング速度に基づく指数平滑
    // steerSpeed が大きいほどレスポンスが速くなる
    float maxDelta = config_.steerSpeed * dt;

    float diff = target - current;
    if (std::abs(diff) < maxDelta) {
        return target;
    }

    return current + ((diff > 0.0f) ? maxDelta : -maxDelta);
}

} // namespace Revora
