#pragma once

#include <cstdint>

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

namespace Revora {

/// ゲームパッドのアナログ軸 (正規化済み -1.0 ~ 1.0)
struct GamepadState {
    float leftStickX  = 0.0f;
    float leftStickY  = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;
    float leftTrigger  = 0.0f;
    float rightTrigger = 0.0f;
    bool  connected    = false;
};

class InputManager {
public:
    InputManager() = default;
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    bool Initialize();
    void Shutdown();

    /// 毎フレーム呼び出し: キーボードとゲームパッドの状態を更新
    void Update();

    bool IsKeyDown(int scancode) const;
    bool IsKeyPressed(int scancode) const;

    const GamepadState& GetGamepadState() const { return gamepad_; }

private:
    static constexpr int kMaxKeys = 512;

    void DetectGamepad();

    const uint8_t*      keyboardState_     = nullptr;
    uint8_t             previousKeys_[kMaxKeys] = {};
    int                 numKeys_           = 0;

    SDL_GameController* controller_        = nullptr;
    GamepadState        gamepad_;
};

} // namespace Revora
