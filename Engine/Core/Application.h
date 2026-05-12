#pragma once

#include "../Platform/Window.h"
#include "../Renderer/GraphicsDevice.h"
#include "../Input/InputManager.h"
#include "Timer.h"

namespace Revora {

class Application {
public:
    Application() = default;
    ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    void Run();
    void Shutdown();

private:
    void FixedUpdate(float dt);
    void Render();

    Window         window_;
    GraphicsDevice graphics_;
    InputManager   input_;
    Timer          timer_;

    bool running_ = false;

    // テスト三角形の回転角 (ラジアン)
    float triangleRotation_ = 0.0f;
};

} // namespace Revora
