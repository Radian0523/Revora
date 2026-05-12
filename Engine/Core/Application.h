#pragma once

#include "../Platform/Window.h"
#include "../Renderer/GraphicsDevice.h"
#include "../Renderer/ShaderManager.h"
#include "../Renderer/DescriptorSetManager.h"
#include "../Renderer/MeshRenderer.h"
#include "../Renderer/Skybox.h"
#include "../Renderer/Camera.h"
#include "../Resource/MeshLoader.h"
#include "../Resource/TextureLoader.h"
#include "../Input/InputManager.h"
#include "../Physics/VehicleConfig.h"
#include "Timer.h"

#include "../../Game/Vehicle/VehicleController.h"
#include "../../Game/Camera/ChaseCameraController.h"
#include "../../Game/Course/CourseData.h"
#include "../../Game/Course/CourseCollider.h"
#include "../../Game/Course/CheckpointSystem.h"
#include "../../Game/Course/LapTimer.h"

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
    Camera               camera_;
    ChaseCameraController chaseCam_;

    // --- 車両 ---
    VehicleController vehicle_;

    // --- コース ---
    CourseData       courseData_;
    CourseCollider   courseCollider_;
    CheckpointSystem checkpointSystem_;
    LapTimer         lapTimer_;

    // --- リソース ---
    MeshResource    cubeMesh_      = {};
    MeshResource    courseMesh_    = {};
    TextureResource checkerTex_    = {};
    TextureResource skyboxCubemap_ = {};

    bool running_ = false;
};

} // namespace Revora
