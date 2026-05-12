#include "InputManager.h"
#include <SDL.h>
#include <cstring>

namespace Revora {

static constexpr float kAxisNormalize = 1.0f / 32767.0f;

InputManager::~InputManager() {
    Shutdown();
}

bool InputManager::Initialize() {
    keyboardState_ = SDL_GetKeyboardState(&numKeys_);
    if (numKeys_ > kMaxKeys) {
        numKeys_ = kMaxKeys;
    }
    std::memset(previousKeys_, 0, sizeof(previousKeys_));

    DetectGamepad();
    return true;
}

void InputManager::Shutdown() {
    if (relativeMouseMode_) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        relativeMouseMode_ = false;
    }

    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
    gamepad_ = {};
}

void InputManager::Update() {
    // 前フレームのキー状態を保存 (押下検出用)
    std::memcpy(previousKeys_, keyboardState_, numKeys_);

    // マウスの相対移動量を取得
    int mx, my;
    SDL_GetRelativeMouseState(&mx, &my);
    mouseDeltaX_ = static_cast<float>(mx);
    mouseDeltaY_ = static_cast<float>(my);

    // ゲームパッドが未接続なら再検出を試みる
    if (!controller_) {
        DetectGamepad();
    }

    // ゲームパッドが切断された場合のハンドリング
    if (controller_ && !SDL_GameControllerGetAttached(controller_)) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        gamepad_ = {};
        DetectGamepad();
    }

    // ゲームパッド軸の読み取り
    if (controller_) {
        gamepad_.connected   = true;
        gamepad_.leftStickX  = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX)  * kAxisNormalize;
        gamepad_.leftStickY  = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY)  * kAxisNormalize;
        gamepad_.rightStickX = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX) * kAxisNormalize;
        gamepad_.rightStickY = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY) * kAxisNormalize;
        gamepad_.leftTrigger  = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  * kAxisNormalize;
        gamepad_.rightTrigger = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) * kAxisNormalize;
    }
}

bool InputManager::IsKeyDown(int scancode) const {
    if (scancode < 0 || scancode >= numKeys_) {
        return false;
    }
    return keyboardState_[scancode] != 0;
}

bool InputManager::IsKeyPressed(int scancode) const {
    if (scancode < 0 || scancode >= numKeys_) {
        return false;
    }
    return keyboardState_[scancode] != 0 && previousKeys_[scancode] == 0;
}

void InputManager::GetMouseDelta(float& dx, float& dy) const {
    dx = mouseDeltaX_;
    dy = mouseDeltaY_;
}

void InputManager::SetRelativeMouseMode(bool enable) {
    SDL_SetRelativeMouseMode(enable ? SDL_TRUE : SDL_FALSE);
    relativeMouseMode_ = enable;
}

void InputManager::DetectGamepad() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                gamepad_.connected = true;
                return;
            }
        }
    }
}

} // namespace Revora
