#include "Application.h"
#include "../Math/MathConstants.h"
#include <SDL.h>

namespace Revora {

static constexpr float kFixedTimeStep  = 1.0f / 60.0f;
static constexpr float kMaxFrameTime   = 0.25f;
static constexpr float kRotationSpeed  = 1.0f;  // rad/s

bool Application::Initialize() {
    WindowDesc desc;
    desc.title  = "Revora";
    desc.width  = 1280;
    desc.height = 720;

    if (!window_.Initialize(desc)) {
        return false;
    }
    if (!graphics_.Initialize(window_)) {
        window_.Shutdown();
        return false;
    }
    if (!input_.Initialize()) {
        graphics_.Shutdown();
        window_.Shutdown();
        return false;
    }

    timer_.Initialize();
    running_ = true;
    return true;
}

void Application::Run() {
    float accumulator = 0.0f;

    while (running_) {
        timer_.Tick();
        float frameTime = timer_.GetDeltaTime();

        // スパイラル・オブ・デス防止
        if (frameTime > kMaxFrameTime) {
            frameTime = kMaxFrameTime;
        }

        // イベントポンプ
        if (window_.PumpEvents()) {
            running_ = false;
            break;
        }

        // 入力更新
        input_.Update();

        // Escape キーで終了
        if (input_.IsKeyDown(SDL_SCANCODE_ESCAPE)) {
            running_ = false;
            break;
        }

        // 固定タイムステップ更新
        accumulator += frameTime;
        while (accumulator >= kFixedTimeStep) {
            FixedUpdate(kFixedTimeStep);
            accumulator -= kFixedTimeStep;
        }

        Render();
    }
}

void Application::Shutdown() {
    input_.Shutdown();
    graphics_.Shutdown();
    window_.Shutdown();
}

void Application::FixedUpdate(float dt) {
    triangleRotation_ += kRotationSpeed * dt;

    // 2pi を超えたら巻き戻す
    if (triangleRotation_ > kTwoPi) {
        triangleRotation_ -= kTwoPi;
    }
}

void Application::Render() {
    // 暗い紺色の背景
    graphics_.BeginFrame(0.05f, 0.05f, 0.15f, 1.0f);

    // MVP 行列を構築
    float aspect = static_cast<float>(window_.GetWidth()) / static_cast<float>(window_.GetHeight());

    Matrix4x4 model = Matrix4x4::RotationY(triangleRotation_);
    Matrix4x4 view  = Matrix4x4::LookAtLH(
        Vector3(0.0f, 0.0f, -2.0f),  // カメラ位置
        Vector3(0.0f, 0.0f, 0.0f),   // 注視点
        Vector3::Up
    );
    Matrix4x4 proj  = Matrix4x4::PerspectiveFovLH(
        kPi / 4.0f,   // 45度
        aspect,
        0.1f,
        100.0f
    );

    // 行ベクトル方式: v' = v * M * V * P
    Matrix4x4 mvp = model * view * proj;

    graphics_.DrawTestTriangle(mvp);
    graphics_.EndFrame();
}

} // namespace Revora
