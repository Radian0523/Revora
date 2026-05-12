#include "VehiclePhysics.h"
#include "Collision.h"
#include "../Math/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Revora {

// 数値安定性のための最小速度閾値
static constexpr float kMinSpeedForSlip = 0.5f;

// 重力加速度
static constexpr float kGravity = 9.81f;

// 地面の定義 (y=0 の無限平面)
static const Vector3 kGroundPlanePoint(0.0f, 0.0f, 0.0f);
static const Vector3 kGroundPlaneNormal(0.0f, 1.0f, 0.0f);

void VehiclePhysics::Initialize(const VehicleConfig& config)
{
    config_ = config;

    body_.SetMass(config_.mass);
    body_.position = config_.spawnPosition;
    body_.rotation = Quaternion::FromAxisAngle(Vector3::Up, config_.spawnYaw);
    body_.velocity        = Vector3::Zero;
    body_.angularVelocity = Vector3::Zero;

    SetupWheelOffsets();

    for (auto& w : wheels_) {
        w = WheelState{};
        w.previousSuspensionLength = config_.suspensionRestLength;
    }
}

void VehiclePhysics::Update(float dt, const VehicleInputState& input)
{
    // 重力
    body_.ApplyForce(Vector3(0.0f, -kGravity * body_.mass, 0.0f));

    // 各ホイールにサスペンション力とタイヤ力を適用
    for (int i = 0; i < kWheelCount; ++i) {
        ApplySuspension(i, dt);
        ApplyTireForces(i, input);
    }

    ApplyDragForces();

    // 積分
    body_.Integrate(dt);

    ClampSpeed();
    ClampToBounds();

    // 地面貫通防止: y 座標が地面 + ホイール半径以下になった場合の補正
    float minY = config_.wheelRadius + config_.suspensionRestLength * 0.3f;
    if (body_.position.y < minY) {
        body_.position.y = minY;
        if (body_.velocity.y < 0.0f) {
            body_.velocity.y = 0.0f;
        }
    }
}

float VehiclePhysics::GetSpeed() const
{
    return body_.velocity.Length();
}

void VehiclePhysics::Reset()
{
    body_.position        = config_.spawnPosition;
    body_.rotation        = Quaternion::FromAxisAngle(Vector3::Up, config_.spawnYaw);
    body_.velocity        = Vector3::Zero;
    body_.angularVelocity = Vector3::Zero;
    body_.forceAccumulator  = Vector3::Zero;
    body_.torqueAccumulator = Vector3::Zero;

    for (auto& w : wheels_) {
        w = WheelState{};
        w.previousSuspensionLength = config_.suspensionRestLength;
    }

    currentSteerAngle_ = 0.0f;
}

void VehiclePhysics::SetupWheelOffsets()
{
    float front = config_.wheelbaseFront;
    float rear  = config_.wheelbaseRear;
    float half  = config_.trackWidth;
    float h     = config_.wheelHeight;

    // 左手座標系: +Z 前方, +X 右方
    wheelLocalOffsets_[kFrontLeft]  = Vector3(-half, h,  front);
    wheelLocalOffsets_[kFrontRight] = Vector3( half, h,  front);
    wheelLocalOffsets_[kRearLeft]   = Vector3(-half, h, -rear);
    wheelLocalOffsets_[kRearRight]  = Vector3( half, h, -rear);
}

void VehiclePhysics::ApplySuspension(int wheelIndex, float dt)
{
    WheelState& wheel = wheels_[wheelIndex];

    // ワールド空間のホイール取り付け位置
    Vector3 wheelWorldPos = body_.LocalToWorld(wheelLocalOffsets_[wheelIndex]);

    // 車体の下方向にレイキャスト
    Vector3 rayDir = -body_.GetUp();
    float maxDist  = config_.suspensionRestLength + config_.wheelRadius;

    HitResult hit = Collision::RayPlane(
        wheelWorldPos, rayDir, kGroundPlanePoint, kGroundPlaneNormal, maxDist);

    if (!hit.hit) {
        wheel.isGrounded       = false;
        wheel.suspensionForce  = 0.0f;
        wheel.previousSuspensionLength = config_.suspensionRestLength;
        return;
    }

    wheel.isGrounded  = true;
    wheel.contactPoint = hit.point;

    // サスペンション長の計算
    float suspensionLength = hit.distance - config_.wheelRadius;
    suspensionLength = std::max(0.0f, suspensionLength);

    // バネ力: 圧縮量に比例する復元力
    float compression = config_.suspensionRestLength - suspensionLength;
    float springForce = config_.suspensionStiffness * compression;

    // ダンパー力: 圧縮速度に比例する減衰力
    float compressionVelocity = (wheel.previousSuspensionLength - suspensionLength) / dt;
    float damperForce = config_.suspensionDamping * compressionVelocity;

    // サスペンションは引っ張り力を発生しない
    float totalForce = std::max(0.0f, springForce + damperForce);

    wheel.suspensionForce          = totalForce;
    wheel.previousSuspensionLength = suspensionLength;

    // 接地法線方向に力を適用
    body_.ApplyForceAtPoint(hit.normal * totalForce, wheelWorldPos);
}

void VehiclePhysics::ApplyTireForces(int wheelIndex, const VehicleInputState& input)
{
    const WheelState& wheel = wheels_[wheelIndex];
    if (!wheel.isGrounded) {
        return;
    }

    bool isFrontWheel = (wheelIndex == kFrontLeft || wheelIndex == kFrontRight);

    // ホイールの前方・右方向をワールド空間で計算
    Vector3 wheelForward = body_.GetForward();
    Vector3 wheelRight   = body_.GetRight();

    // 前輪はステアリング角で回転
    if (isFrontWheel) {
        float cosA = std::cos(currentSteerAngle_);
        float sinA = std::sin(currentSteerAngle_);
        Vector3 fwd = wheelForward;
        Vector3 rgt = wheelRight;
        wheelForward = fwd * cosA + rgt * sinA;
        wheelRight   = rgt * cosA - fwd * sinA;
    }

    // 接地点における速度
    Vector3 wheelWorldPos = body_.LocalToWorld(wheelLocalOffsets_[wheelIndex]);
    Vector3 pointVelocity = body_.GetPointVelocity(wheelWorldPos);

    // 横・縦方向の速度成分
    float lateralSpeed     = Vector3::Dot(pointVelocity, wheelRight);
    float longitudinalSpeed = Vector3::Dot(pointVelocity, wheelForward);

    // --- 横力 (コーナリングフォース) ---
    // スリップ角を計算し、簡易 Pacejka カーブで横力係数を得る
    float slipAngle = std::atan2(lateralSpeed,
                                  std::abs(longitudinalSpeed) + kMinSpeedForSlip);

    float lateralForceCoeff = CalculateLateralForce(slipAngle);
    float lateralForceMag   = lateralForceCoeff * wheel.suspensionForce;

    // 横速度と逆方向に復元力として適用
    body_.ApplyForceAtPoint(-wheelRight * lateralForceMag, wheelWorldPos);

    // --- 縦力 (駆動力 / ブレーキ力) ---
    // 後輪駆動: 駆動力は後輪のみ、ブレーキは全輪
    float longitudinalForce = 0.0f;

    if (!isFrontWheel) {
        longitudinalForce += input.throttle * config_.engineTorque;
    }

    // ブレーキ力は速度と逆方向に作用する
    if (input.brake > 0.0f) {
        float brakeDir = (longitudinalSpeed > kEpsilon) ? -1.0f :
                         (longitudinalSpeed < -kEpsilon) ? 1.0f : 0.0f;
        longitudinalForce += brakeDir * input.brake * config_.brakeTorque;
    }

    body_.ApplyForceAtPoint(wheelForward * longitudinalForce, wheelWorldPos);
}

void VehiclePhysics::ApplyDragForces()
{
    float speed = body_.velocity.Length();
    if (speed < kEpsilon) {
        return;
    }

    // 空気抵抗: 速度の二乗に比例 (高速域で強く効く)
    Vector3 dragForce = -body_.velocity * (config_.dragCoefficient * speed);
    body_.ApplyForce(dragForce);

    // 転がり抵抗: 接地時のみ、一定の抵抗力
    bool anyWheelGrounded = false;
    for (const auto& w : wheels_) {
        if (w.isGrounded) {
            anyWheelGrounded = true;
            break;
        }
    }

    if (anyWheelGrounded && speed > kEpsilon) {
        Vector3 rollingForce = -body_.velocity.Normalized() * config_.rollingResistance;
        body_.ApplyForce(rollingForce);
    }
}

void VehiclePhysics::ClampSpeed()
{
    float speed = body_.velocity.Length();
    if (speed > config_.maxSpeed) {
        body_.velocity = body_.velocity.Normalized() * config_.maxSpeed;
    }
}

void VehiclePhysics::ClampToBounds()
{
    body_.position = Collision::ClampToBounds(body_.position, kArenaHalfExtent);
}

float VehiclePhysics::CalculateLateralForce(float slipAngle) const
{
    // 簡易 Pacejka: スリップ角を閾値で正規化し、sin カーブで横力を返す
    // 閾値を超えるとグリップが飽和し、それ以上滑ってもグリップは増えない
    float thresholdRad = config_.slipAngleThreshold * kDegToRad;
    float normalized   = slipAngle / thresholdRad;
    return config_.tireGripFactor
         * std::sin(std::clamp(normalized, -1.0f, 1.0f) * kHalfPi);
}

} // namespace Revora
