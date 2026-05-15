#pragma once

#include "RigidBody.h"
#include "VehicleConfig.h"
#include "../Math/Vector3.h"

#include <array>

namespace Revora {

/// 車両の入力状態 (Game 層の InputMapper から渡される)
/// 値域は全て 0.0 ~ 1.0 (ステアリングのみ -1.0 ~ 1.0)
struct VehicleInputState {
    float throttle  = 0.0f;  // 0.0 ~ 1.0
    float brake     = 0.0f;  // 0.0 ~ 1.0
    float steering  = 0.0f;  // -1.0 (左) ~ 1.0 (右)
};

/// ホイールのインデックス定義
enum WheelIndex : int {
    kFrontLeft  = 0,
    kFrontRight = 1,
    kRearLeft   = 2,
    kRearRight  = 3,
    kWheelCount = 4
};

/// 各ホイールの物理状態 (フレーム間で保持される)
struct WheelState {
    float previousSuspensionLength = 0.0f;  // 前フレームのサスペンション長 (減衰計算用)
    bool  isGrounded               = false;
    float suspensionForce          = 0.0f;  // 今フレームの接地荷重
    Vector3 contactPoint;                   // ワールド空間の接地点
};

/// 車両物理シミュレーション本体
/// レイキャストサスペンション + 簡易 Pacejka タイヤモデルを使用
class VehiclePhysics {
public:
    void Initialize(const VehicleConfig& config);

    /// 物理シミュレーションの1ステップ (固定タイムステップで呼ばれる)
    void Update(float dt, const VehicleInputState& input);

    /// 現在のステアリング角を設定する (ラジアン)
    void SetSteerAngle(float angle) { currentSteerAngle_ = angle; }

    /// 車体の剛体状態への参照
    RigidBody&       GetBody()       { return body_; }
    const RigidBody& GetBody() const { return body_; }

    /// 現在のステアリング角 (ラジアン)
    float GetSteerAngle() const { return currentSteerAngle_; }

    /// 現在の速度 (m/s)
    float GetSpeed() const;

    /// ホイール状態の取得
    const std::array<WheelState, kWheelCount>& GetWheelStates() const { return wheels_; }

    /// 矩形境界クランプの有効/無効を切り替える
    /// コースシステム使用時は CourseCollider が境界管理するため無効にする
    void SetBoundaryClampEnabled(bool enabled) { enableBoundaryClamp_ = enabled; }

    /// スポーン位置にリセットする
    void Reset();

private:
    /// 各ホイールのローカル座標オフセットを初期化する
    void SetupWheelOffsets();

    /// 1輪分のサスペンション力を計算して適用する
    void ApplySuspension(int wheelIndex, float dt);

    /// 接地しているホイールにタイヤ力を適用する
    void ApplyTireForces(int wheelIndex, const VehicleInputState& input);

    /// 空気抵抗・転がり抵抗を適用する
    void ApplyDragForces();

    /// 最高速制限のクランプ
    void ClampSpeed();

    /// 境界チェック
    void ClampToBounds();

    /// 簡易 Pacejka カーブによる横力係数
    /// スリップ角に対して sin カーブで横力を返す
    float CalculateLateralForce(float slipAngle) const;

    VehicleConfig config_;
    RigidBody     body_;

    std::array<Vector3, kWheelCount>    wheelLocalOffsets_;
    std::array<WheelState, kWheelCount> wheels_;

    float currentSteerAngle_   = 0.0f;
    bool  enableBoundaryClamp_ = true;

    static constexpr float kArenaHalfExtent = 50.0f;
};

} // namespace Revora
