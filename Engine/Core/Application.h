#pragma once

#include "../Platform/Window.h"
#include "../Renderer/GraphicsDevice.h"
#include "../Renderer/ShaderManager.h"
#include "../Renderer/DescriptorSetManager.h"
#include "../Renderer/MeshRenderer.h"
#include "../Renderer/Skybox.h"
#include "../Renderer/Camera.h"
#include "../Renderer/ShadowMap.h"
#include "../Renderer/ParticleRenderer.h"
#include "../Renderer/SpriteRenderer.h"
#include "../Resource/MeshLoader.h"
#include "../Resource/TextureLoader.h"
#include "../Input/InputManager.h"
#include "../Physics/VehicleConfig.h"
#include "../Audio/AudioManager.h"
#include "Timer.h"
#include "LinearAllocator.h"

#include "../../Game/Vehicle/VehicleController.h"
#include "../../Game/Camera/ChaseCameraController.h"
#include "../../Game/Course/CourseData.h"
#include "../../Game/Course/CourseCollider.h"
#include "../../Game/Race/RaceManager.h"
#include "../../Game/Race/GhostRecorder.h"
#include "../../Game/Flow/GameFlowManager.h"
#include "../../Game/Particle/ParticleEmitter.h"
#include "../../Game/UI/HudOverlay.h"

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

    /// GameFlowManager に各状態のコールバックを登録する
    void SetupFlowCallbacks();

    /// パーティクル生成: タイヤスモークと壁衝突火花
    void EmitParticles(float dt);

    // --- コアシステム ---
    Window         window_;
    GraphicsDevice graphics_;
    InputManager   input_;
    Timer          timer_;
    AudioManager   audio_;
    LinearAllocator frameAllocator_;

    // --- 描画システム ---
    ShaderManager        shaderMgr_;
    DescriptorSetManager descriptorMgr_;
    MeshRenderer         meshRenderer_;
    Skybox               skybox_;
    ShadowMap            shadowMap_;
    ParticleRenderer     particleRenderer_;
    SpriteRenderer       spriteRenderer_;

    // --- リソースローダー ---
    MeshLoader    meshLoader_;
    TextureLoader textureLoader_;

    // --- カメラ ---
    Camera               camera_;
    ChaseCameraController chaseCam_;

    // --- 車両 ---
    VehicleController vehicle_;

    // --- コース ---
    CourseData     courseData_;
    CourseCollider courseCollider_;

    // --- レース管理 ---
    RaceManager     raceManager_;
    GhostRecorder   ghostRecorder_;
    GhostRecorder   ghostPlayback_;  // 再生用 (前回記録の読み込み先)
    GameFlowManager flowManager_;

    // --- パーティクル ---
    ParticleEmitter smokeEmitter_;
    ParticleEmitter sparkEmitter_;

    // --- UI ---
    HudOverlay hudOverlay_;

    // --- リソース ---
    MeshResource    cubeMesh_      = {};
    MeshResource    courseMesh_    = {};
    TextureResource checkerTex_    = {};
    TextureResource skyboxCubemap_ = {};
    TextureResource fontAtlas_     = {};

    bool running_ = false;

    // カウントダウン SE のエッジ検出: 秒が切り替わったときだけ SE を鳴らす
    int prevCountdownSeconds_ = -1;

    // ラップ完了 SE のエッジ検出: 前フレームのラップ番号を記憶する
    int prevLap_ = 0;

    // ゴースト保存パス
    static constexpr const char* kGhostFilePath = "Assets/Data/ghost_default.bin";

    // フレーム単位アロケータの容量 (UI 頂点バッファの一時確保に十分)
    static constexpr std::size_t kFrameAllocatorCapacity = 64 * 1024;
};

} // namespace Revora
