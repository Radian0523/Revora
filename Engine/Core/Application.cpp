#include "Application.h"
#include "../Math/MathConstants.h"
#include <SDL.h>

namespace Revora {

static constexpr float kFixedTimeStep  = 1.0f / 60.0f;
static constexpr float kMaxFrameTime   = 0.25f;
static constexpr float kRotationSpeed  = 0.5f;  // rad/s

// ディスクリプタセット数: MeshRenderer (2 フレーム) + Skybox (2 フレーム)
static constexpr uint32_t kMaxDescriptorSets = 16;

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

    // --- 描画システム初期化 ---
    VkDevice device               = graphics_.GetDevice();
    VkPhysicalDevice physDevice   = graphics_.GetPhysicalDevice();
    VkRenderPass renderPass       = graphics_.GetRenderPass();
    VkCommandPool commandPool     = graphics_.GetCommandPool();
    VkQueue graphicsQueue         = graphics_.GetGraphicsQueue();

    shaderMgr_.Initialize(device);

    if (!descriptorMgr_.Initialize(device, kMaxDescriptorSets)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize DescriptorSetManager");
        Shutdown();
        return false;
    }

    if (!meshRenderer_.Initialize(device, physDevice, renderPass, descriptorMgr_, shaderMgr_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize MeshRenderer");
        Shutdown();
        return false;
    }

    if (!skybox_.Initialize(device, physDevice, renderPass, descriptorMgr_, shaderMgr_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Skybox");
        Shutdown();
        return false;
    }

    // --- リソースローダー初期化 ---
    meshLoader_.Initialize(device, physDevice, commandPool, graphicsQueue);
    textureLoader_.Initialize(device, physDevice, commandPool, graphicsQueue);

    // --- テストリソース読み込み ---
    if (!meshLoader_.LoadMesh("Assets/Models/Cube.obj", testMesh_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load test mesh");
        Shutdown();
        return false;
    }

    if (!textureLoader_.LoadTexture2D("Assets/Textures/Checkerboard.png", testTexture_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load test texture");
        Shutdown();
        return false;
    }

    std::string skyboxFaces[6] = {
        "Assets/Textures/Skybox/right.png",
        "Assets/Textures/Skybox/left.png",
        "Assets/Textures/Skybox/top.png",
        "Assets/Textures/Skybox/bottom.png",
        "Assets/Textures/Skybox/front.png",
        "Assets/Textures/Skybox/back.png",
    };
    if (!textureLoader_.LoadCubemap(skyboxFaces, skyboxCubemap_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load skybox cubemap");
        Shutdown();
        return false;
    }

    // --- カメラ初期化 ---
    camera_.SetPosition(Vector3(0.0f, 1.0f, -3.0f));
    camera_.SetRotation(0.0f, 0.0f);
    cameraController_.Initialize(camera_);

    // 相対マウスモードを有効化 (カーソル非表示 + マウスキャプチャ)
    input_.SetRelativeMouseMode(true);

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
    if (graphics_.GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(graphics_.GetDevice());
    }

    textureLoader_.DestroyTexture(skyboxCubemap_);
    textureLoader_.DestroyTexture(testTexture_);
    meshLoader_.DestroyMesh(testMesh_);

    skybox_.Shutdown();
    meshRenderer_.Shutdown();
    descriptorMgr_.Shutdown();
    shaderMgr_.Shutdown();

    input_.Shutdown();
    graphics_.Shutdown();
    window_.Shutdown();
}

void Application::FixedUpdate(float dt) {
    // テストメッシュを緩やかに回転
    meshRotation_ += kRotationSpeed * dt;
    if (meshRotation_ > kTwoPi) {
        meshRotation_ -= kTwoPi;
    }

    // カメラ操作
    cameraController_.Update(input_, dt);
}

void Application::Render() {
    graphics_.BeginFrame(0.05f, 0.05f, 0.15f, 1.0f);

    uint32_t frameIndex = graphics_.GetCurrentFrame();
    VkCommandBuffer cmd = graphics_.GetCurrentCommandBuffer();

    float aspect = static_cast<float>(window_.GetWidth())
                 / static_cast<float>(window_.GetHeight());

    Matrix4x4 view = camera_.GetViewMatrix();
    Matrix4x4 proj = camera_.GetProjectionMatrix(aspect);

    // --- スカイボックス描画 (メッシュの前に描画、デプス書き込み無効) ---
    skybox_.UpdateSceneData(frameIndex, view, proj);
    skybox_.Draw(cmd, frameIndex, skyboxCubemap_);

    // --- メッシュ描画 ---
    // Lambert ライティング用の光源方向 (太陽光: 斜め上方から)
    float lightDir[3] = {0.5f, 1.0f, 0.3f};
    meshRenderer_.UpdateSceneData(frameIndex, view, proj, lightDir);

    Matrix4x4 modelMatrix = Matrix4x4::RotationY(meshRotation_);
    meshRenderer_.Draw(cmd, frameIndex, testMesh_, testTexture_, modelMatrix);

    graphics_.EndFrame();
}

} // namespace Revora
