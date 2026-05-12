#pragma once

#include "../../Engine/Physics/VehiclePhysics.h"

namespace Revora {

class InputManager;

/// キーボード/ゲームパッドの生入力を VehicleInputState に変換する
/// 車両固有のキーマッピングは Game 層のロジックであるため、
/// Engine 層の InputManager とは分離して配置している
class VehicleInputMapper {
public:
    /// InputManager の状態を読み取り、VehicleInputState を返す
    /// キーボードはデジタル入力 (0 or 1)、ゲームパッドはアナログ入力 (0~1)
    /// 両方が同時に入力された場合はより大きい値を採用する
    static VehicleInputState MapInput(const InputManager& input);
};

} // namespace Revora
