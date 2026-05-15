#include "Application.h"
#include "../Math/MathConstants.h"
#include "../Math/Matrix4x4.h"
#include "../../Game/Course/CourseMeshGenerator.h"

#include <SDL.h>

#include <cmath>

namespace Revora {

static constexpr float kFixedTimeStep  = 1.0f / 60.0f;
static constexpr float kMaxFrameTime   = 0.25f;

// ディスクリプタセット数: MeshRenderer (2 フレーム) + Skybox (2 フレーム)
static constexpr uint32_t kMaxDescriptorSets = 16;

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

    // --- コース読み込み ---
    if (!courseData_.LoadFromFile("Assets/Data/DefaultCourse.json")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load course data");
        Shutdown();
        return false;
    }
    courseData_.BuildSpline();

    // コースメッシュのプロシージャル生成
    {
        std::vector<Vertex> courseVertices;
        std::vector<uint32_t> courseIndices;
        CourseMeshGenerator::Generate(
            courseData_.GetSpline(), courseData_.trackWidth,
            courseVertices, courseIndices);

        if (!meshLoader_.CreateMesh(courseVertices, courseIndices, courseMesh_)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create course mesh");
            Shutdown();
            return false;
        }
        SDL_Log("Course mesh created: %zu vertices, %zu indices",
                courseVertices.size(), courseIndices.size());
    }

    // --- コースシステム初期化 ---
    courseCollider_.Initialize(&courseData_.GetSpline(), courseData_.trackWidth);

    // --- レース管理初期化 ---
    raceManager_.Initialize(
        &courseData_.GetSpline(),
        courseData_.checkpointPositions,
        courseData_.lapCount);

    // --- ゴースト初期化 ---
    // 前回の記録があれば再生用に読み込む
    ghostPlayback_.LoadFromFile(kGhostFilePath);

    // --- 車両初期化 ---
    VehicleConfig vehicleConfig;
    vehicleConfig.LoadFromFile("Assets/Data/DefaultVehicle.json");

    // スポーン位置をコース上に設定
    Vector3 spawnPos = courseData_.GetSpline().Evaluate(courseData_.spawnT);
    Vector3 spawnDir = courseData_.GetSpline().EvaluateTangent(courseData_.spawnT);
    spawnPos.y = 0.5f;  // 地面からの高さ
    vehicleConfig.spawnPosition = spawnPos;
    vehicleConfig.spawnYaw = std::atan2(spawnDir.x, spawnDir.z);

    if (!vehicle_.Initialize(vehicleConfig)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize vehicle");
        Shutdown();
        return false;
    }

    // コース境界拘束を車両に設定
    vehicle_.SetCourseCollider(&courseCollider_);

    // --- カメラ初期化 ---
    chaseCam_.Initialize(camera_);

    // --- ゲームフロー初期化 ---
    SetupFlowCallbacks();
    flowManager_.Initialize(GameState::Countdown);

    // 車両操作は相対マウス不要 (マウスは使わない)
    input_.SetRelativeMouseMode(false);

    timer_.Initialize();
    running_ = true;
    return true;
}

void Application::SetupFlowCallbacks()
{
    // --- Countdown 状態 ---
    flowManager_.SetCallbacks(GameState::Countdown, {
        /*onEnter*/  [this]() {
            raceManager_.Reset();
            ghostRecorder_.StartRecording();
        },
        /*onUpdate*/ [this](float /*dt*/) {
            // RaceManager がカウントダウンを管理し、Racing に遷移したら通知
            if (raceManager_.GetState() == RaceState::Racing) {
                flowManager_.TransitionTo(GameState::Racing);
            }
        },
        /*onExit*/   nullptr
    });

    // --- Racing 状態 ---
    flowManager_.SetCallbacks(GameState::Racing, {
        /*onEnter*/  nullptr,
        /*onUpdate*/ [this](float /*dt*/) {
            if (raceManager_.GetState() == RaceState::Finished) {
                flowManager_.TransitionTo(GameState::Finished);
            }
        },
        /*onExit*/   nullptr
    });

    // --- Finished 状態 ---
    flowManager_.SetCallbacks(GameState::Finished, {
        /*onEnter*/  [this]() {
            // ゴースト記録を停止し、ファイルに保存する
            ghostRecorder_.StopRecording();
            ghostRecorder_.SaveToFile(kGhostFilePath);

            // 保存した記録を次回再生用にコピー
            ghostPlayback_.LoadFromFile(kGhostFilePath);
        },
        /*onUpdate*/ nullptr,
        /*onExit*/   nullptr
    });
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

        // R キーでリセット (車両 + レース + ゴースト記録を一括リセット)
        if (input_.IsKeyPressed(SDL_SCANCODE_R)) {
            vehicle_.Reset();
            raceManager_.Reset();
            ghostRecorder_.Reset();
            flowManager_.TransitionTo(GameState::Countdown);
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
    meshLoader_.DestroyMesh(courseMesh_);
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
    const RigidBody& body = vehicle_.GetPhysics().GetBody();

    // RaceManager 更新 (カウントダウン / チェックポイント / タイマー)
    if (flowManager_.ShouldUpdateRace()) {
        raceManager_.Update(dt, body.position);
    }

    // GameFlowManager 更新 (状態遷移コールバック)
    flowManager_.Update(dt);

    // 車両更新 (Racing 中のみ入力を受け付ける)
    if (flowManager_.ShouldAcceptInput()) {
        vehicle_.Update(input_, dt);

        // ゴースト記録: レース中の車両状態をサンプリング
        ghostRecorder_.RecordFrame(
            raceManager_.GetTotalTime(),
            body.position,
            body.rotation,
            vehicle_.GetPhysics().GetSteerAngle(),
            vehicle_.GetPhysics().GetSpeed());
    }

    // 追従カメラ更新
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

    // コース路面: プロシージャル生成したメッシュをそのまま描画
    Matrix4x4 courseModel = Matrix4x4::Identity();
    meshRenderer_.Draw(cmd, frameIndex, courseMesh_, checkerTex_, courseModel);

    // 車両: 剛体の位置と回転からモデル行列を構築
    const RigidBody& body = vehicle_.GetPhysics().GetBody();
    Matrix4x4 vehicleTranslation = Matrix4x4::Translation(body.position);
    Matrix4x4 vehicleRotation    = body.rotation.ToMatrix();
    Matrix4x4 vehicleScale       = Matrix4x4::Scaling(1.0f, 0.5f, 2.0f);
    Matrix4x4 vehicleModel       = vehicleScale * vehicleRotation * vehicleTranslation;
    meshRenderer_.Draw(cmd, frameIndex, cubeMesh_, checkerTex_, vehicleModel);

    // ゴースト車両: 再生データがあれば半透明風に通常メッシュで仮描画
    if (ghostPlayback_.HasData() && flowManager_.ShouldUpdateRace()) {
        GhostPlaybackState ghost = ghostPlayback_.Sample(raceManager_.GetTotalTime());
        if (ghost.isValid) {
            Matrix4x4 ghostTranslation = Matrix4x4::Translation(ghost.position);
            Matrix4x4 ghostRotation    = ghost.rotation.ToMatrix();
            Matrix4x4 ghostScale       = Matrix4x4::Scaling(0.9f, 0.45f, 1.8f);
            Matrix4x4 ghostModel       = ghostScale * ghostRotation * ghostTranslation;
            meshRenderer_.Draw(cmd, frameIndex, cubeMesh_, checkerTex_, ghostModel);
        }
    }

    graphics_.EndFrame();
}

} // namespace Revora
