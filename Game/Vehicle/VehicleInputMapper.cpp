#include "VehicleInputMapper.h"
#include "../../Engine/Input/InputManager.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace Revora {

// ゲームパッドのアナログ入力デッドゾーン
static constexpr float kDeadZone = 0.15f;

/// デッドゾーンを適用して 0~1 に再マッピングする
static float ApplyDeadZone(float value)
{
    float absVal = std::abs(value);
    if (absVal < kDeadZone) {
        return 0.0f;
    }
    // デッドゾーン分を差し引いて 0~1 に再マッピング
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    return sign * (absVal - kDeadZone) / (1.0f - kDeadZone);
}

VehicleInputState VehicleInputMapper::MapInput(const InputManager& input)
{
    VehicleInputState state;

    // --- キーボード入力 ---
    float kbThrottle = input.IsKeyDown(SDL_SCANCODE_W) ? 1.0f : 0.0f;
    float kbBrake    = input.IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f;
    float kbSteerL   = input.IsKeyDown(SDL_SCANCODE_A) ? -1.0f : 0.0f;
    float kbSteerR   = input.IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f;
    float kbSteering = kbSteerL + kbSteerR;

    // --- ゲームパッド入力 ---
    const GamepadState& pad = input.GetGamepadState();
    float gpThrottle = 0.0f;
    float gpBrake    = 0.0f;
    float gpSteering = 0.0f;

    if (pad.connected) {
        gpThrottle = std::max(0.0f, pad.rightTrigger);
        gpBrake    = std::max(0.0f, pad.leftTrigger);
        gpSteering = ApplyDeadZone(pad.leftStickX);
    }

    // キーボードとゲームパッドの大きい方を採用
    state.throttle = std::max(kbThrottle, gpThrottle);
    state.brake    = std::max(kbBrake,    gpBrake);

    // ステアリングは絶対値が大きい方を採用
    state.steering = (std::abs(kbSteering) > std::abs(gpSteering))
                   ? kbSteering : gpSteering;

    return state;
}

} // namespace Revora
