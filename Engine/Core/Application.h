#pragma once

#include "../Platform/Window.h"
#include "../Renderer/GraphicsDevice.h"
#include "../Renderer/ShaderManager.h"
#include "../Renderer/DescriptorSetManager.h"
#include "../Renderer/MeshRenderer.h"
#include "../Renderer/Skybox.h"
#include "../Renderer/Camera.h"
#include "../Renderer/DebugCameraController.h"
#include "../Resource/MeshLoader.h"
#include "../Resource/TextureLoader.h"
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

    // --- コアシステム ---
    Window         window_;
    GraphicsDevice graphics_;
    InputManager   input_;
    Timer          timer_;

    // --- 描画システム ---
    ShaderManager        shaderMgr_;
    DescriptorSetManager descriptorMgr_;
    MeshRenderer         meshRenderer_;
    Skybox               skybox_;

    // --- リソースローダー ---
    MeshLoader    meshLoader_;
    TextureLoader textureLoader_;

    // --- カメラ ---
    Camera                camera_;
    DebugCameraController cameraController_;

    // --- テストリソース ---
    MeshResource    testMesh_ = {};
    TextureResource testTexture_ = {};
    TextureResource skyboxCubemap_ = {};

    bool  running_ = false;
    float meshRotation_ = 0.0f;
};

} // namespace Revora
