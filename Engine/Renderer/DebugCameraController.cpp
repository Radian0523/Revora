#include "DebugCameraController.h"
#include "../Input/InputManager.h"

#include <SDL.h>

namespace Revora {

void DebugCameraController::Initialize(Camera& camera) {
    camera_ = &camera;
}

void DebugCameraController::Update(const InputManager& input, float deltaTime) {
    if (!camera_) {
        return;
    }

    // --- マウス回転 ---
    float dx, dy;
    input.GetMouseDelta(dx, dy);

    float yaw   = camera_->GetYaw()   + dx * lookSensitivity_;
    float pitch = camera_->GetPitch() - dy * lookSensitivity_;
    camera_->SetRotation(pitch, yaw);

    // --- キーボード移動 ---
    Vector3 forward = camera_->GetForward();
    Vector3 right   = camera_->GetRight();
    Vector3 movement = Vector3::Zero;

    if (input.IsKeyDown(SDL_SCANCODE_W)) { movement += forward; }
    if (input.IsKeyDown(SDL_SCANCODE_S)) { movement -= forward; }
    if (input.IsKeyDown(SDL_SCANCODE_D)) { movement += right; }
    if (input.IsKeyDown(SDL_SCANCODE_A)) { movement -= right; }
    if (input.IsKeyDown(SDL_SCANCODE_SPACE))  { movement += Vector3::Up; }
    if (input.IsKeyDown(SDL_SCANCODE_LSHIFT)) { movement -= Vector3::Up; }

    // 斜め移動の正規化
    if (movement.LengthSquared() > 0.0f) {
        movement = movement.Normalized();
    }

    Vector3 newPos = camera_->GetPosition() + movement * moveSpeed_ * deltaTime;
    camera_->SetPosition(newPos);
}

} // namespace Revora
