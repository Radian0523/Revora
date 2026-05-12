#include "Application.h"
#include "../Math/MathConstants.h"
#include "../Math/Matrix4x4.h"

#include <SDL.h>

namespace Revora {

static constexpr float kFixedTimeStep  = 1.0f / 60.0f;
static constexpr float kMaxFrameTime   = 0.25f;

// ディスクリプタセット数: MeshRenderer (2 フレーム) + Skybox (2 フレーム)
static constexpr uint32_t kMaxDescriptorSets = 16;

// 地面メッシュのスケーリング (Cube.obj を薄く広げて平面に見立てる)
static constexpr float kGroundScaleXZ = 50.0f;
static constexpr float kGroundScaleY  = 0.05f;

bool Application::Initialize()
{
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
    VkDevice device             = graphics_.GetDevice();
    VkPhysicalDevice physDevice = graphics_.GetPhysicalDevice();
    VkRenderPass renderPass     = graphics_.GetRenderPass();
    VkCommandPool commandPool   = graphics_.GetCommandPool();
    VkQueue graphicsQueue       = graphics_.GetGraphicsQueue();

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

    // --- リソース読み込み ---
    if (!meshLoader_.LoadMesh("Assets/Models/Cube.obj", cubeMesh_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load cube mesh");
        Shutdown();
        return false;
    }

    if (!textureLoader_.LoadTexture2D("Assets/Textures/Checkerboard.png", checkerTex_)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load checkerboard texture");
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

    // --- 車両初期化 ---
    VehicleConfig vehicleConfig;
    vehicleConfig.LoadFromFile("Assets/Data/DefaultVehicle.json");

    if (!vehicle_.Initialize(vehicleConfig)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize vehicle");
        Shutdown();
        return false;
    }

    // --- カメラ初期化 ---
    chaseCam_.Initialize(camera_);

    // 車両操作は相対マウス不要 (マウスは使わない)
    input_.SetRelativeMouseMode(false);

    timer_.Initialize();
    running_ = true;
    return true;
}

void Application::Run()
{
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

void Application::Shutdown()
{
    if (graphics_.GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(graphics_.GetDevice());
    }

    textureLoader_.DestroyTexture(skyboxCubemap_);
    textureLoader_.DestroyTexture(checkerTex_);
    meshLoader_.DestroyMesh(cubeMesh_);

    skybox_.Shutdown();
    meshRenderer_.Shutdown();
    descriptorMgr_.Shutdown();
    shaderMgr_.Shutdown();

    input_.Shutdown();
    graphics_.Shutdown();
    window_.Shutdown();
}

void Application::FixedUpdate(float dt)
{
    // 車両更新 (入力 → 物理シミュレーション)
    vehicle_.Update(input_, dt);

    // 追従カメラ更新
    const RigidBody& body = vehicle_.GetPhysics().GetBody();
    chaseCam_.Update(
        body.position,
        body.rotation,
        vehicle_.GetPhysics().GetSpeed(),
        dt
    );
}

void Application::Render()
{
    graphics_.BeginFrame(0.05f, 0.05f, 0.15f, 1.0f);

    uint32_t frameIndex = graphics_.GetCurrentFrame();
    VkCommandBuffer cmd = graphics_.GetCurrentCommandBuffer();

    float aspect = static_cast<float>(window_.GetWidth())
                 / static_cast<float>(window_.GetHeight());

    Matrix4x4 view = camera_.GetViewMatrix();
    Matrix4x4 proj = camera_.GetProjectionMatrix(aspect);

    // --- スカイボックス ---
    skybox_.UpdateSceneData(frameIndex, view, proj);
    skybox_.Draw(cmd, frameIndex, skyboxCubemap_);

    // --- メッシュ描画 ---
    float lightDir[3] = {0.5f, 1.0f, 0.3f};
    meshRenderer_.UpdateSceneData(frameIndex, view, proj, lightDir);

    // 地面: Cube.obj を薄く伸ばして平面に見立てる
    Matrix4x4 groundModel = Matrix4x4::Scaling(kGroundScaleXZ, kGroundScaleY, kGroundScaleXZ);
    meshRenderer_.Draw(cmd, frameIndex, cubeMesh_, checkerTex_, groundModel);

    // 車両: 剛体の位置と回転からモデル行列を構築
    const RigidBody& body = vehicle_.GetPhysics().GetBody();
    Matrix4x4 vehicleTranslation = Matrix4x4::Translation(body.position);
    Matrix4x4 vehicleRotation    = body.rotation.ToMatrix();
    Matrix4x4 vehicleScale       = Matrix4x4::Scaling(1.0f, 0.5f, 2.0f);
    Matrix4x4 vehicleModel       = vehicleScale * vehicleRotation * vehicleTranslation;
    meshRenderer_.Draw(cmd, frameIndex, cubeMesh_, checkerTex_, vehicleModel);

    graphics_.EndFrame();
}

} // namespace Revora
