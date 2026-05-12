#pragma once

#include "Camera.h"

namespace Revora {

class InputManager;

/// WASD + マウスのフリーカメラコントローラー (Phase 2 デバッグ用)
/// Phase 3 以降は Game 層の CameraController に置き換わる
class DebugCameraController {
public:
    DebugCameraController() = default;

    void Initialize(Camera& camera);

    /// 毎フレーム呼び出し: キーボードとマウスの入力からカメラを操作する
    void Update(const InputManager& input, float deltaTime);

    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetLookSensitivity(float sensitivity) { lookSensitivity_ = sensitivity; }

private:
    Camera* camera_ = nullptr;
    float moveSpeed_       = 5.0f;
    float lookSensitivity_ = 0.003f;
};

} // namespace Revora
