cmake_minimum_required(VERSION 3.20)
project(Revora LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDL2 検出 ---
find_package(SDL2 REQUIRED)

# --- Vulkan 検出 ---
find_package(Vulkan REQUIRED)

# --- Assimp 検出 ---
find_package(assimp REQUIRED)

# --- nlohmann/json 検出 ---
find_package(nlohmann_json REQUIRED)

# --- SPIR-V シェーダーコンパイル ---
# glslc を検索 (Vulkan SDK に同梱)
find_program(GLSLC_EXECUTABLE glslc HINTS
    "$ENV{VULKAN_SDK}/bin"
    "${Vulkan_INCLUDE_DIR}/../bin"
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc が見つかりません。Vulkan SDK をインストールしてください。")
endif()

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Assets/Shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/Triangle.vert"
    "${SHADER_SOURCE_DIR}/Triangle.frag"
    "${SHADER_SOURCE_DIR}/Mesh.vert"
    "${SHADER_SOURCE_DIR}/Mesh.frag"
    "${SHADER_SOURCE_DIR}/Skybox.vert"
    "${SHADER_SOURCE_DIR}/Skybox.frag"
    "${SHADER_SOURCE_DIR}/ShadowDepth.vert"
    "${SHADER_SOURCE_DIR}/ShadowDepth.frag"
    "${SHADER_SOURCE_DIR}/Particle.vert"
    "${SHADER_SOURCE_DIR}/Particle.frag"
    "${SHADER_SOURCE_DIR}/Sprite.vert"
    "${SHADER_SOURCE_DIR}/Sprite.frag"
)

set(SHADER_SPIRV_OUTPUTS "")
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER_SRC} -o ${SHADER_SPV}
        DEPENDS ${SHADER_SRC}
        COMMENT "SPIR-V コンパイル: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
    )
    list(APPEND SHADER_SPIRV_OUTPUTS ${SHADER_SPV})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_SPIRV_OUTPUTS})

# --- Engine 静的ライブラリ ---
set(ENGINE_SOURCES
    Engine/Math/Vector3.cpp
    Engine/Math/Matrix4x4.cpp
    Engine/Math/Quaternion.cpp
    Engine/Math/CatmullRomSpline.cpp
    Engine/Platform/Window.cpp
    Engine/Core/Timer.cpp
    Engine/Core/Application.cpp
    Engine/Core/LinearAllocator.cpp
    Engine/Audio/AudioManager.cpp
    Engine/Renderer/GraphicsDevice.cpp
    Engine/Renderer/VulkanHelpers.cpp
    Engine/Renderer/Vertex.cpp
    Engine/Renderer/ShaderManager.cpp
    Engine/Renderer/PipelineBuilder.cpp
    Engine/Renderer/DescriptorSetManager.cpp
    Engine/Renderer/MeshRenderer.cpp
    Engine/Renderer/Camera.cpp
    Engine/Renderer/DebugCameraController.cpp
    Engine/Renderer/Skybox.cpp
    Engine/Renderer/ShadowMap.cpp
    Engine/Renderer/ParticleRenderer.cpp
    Engine/Renderer/SpriteRenderer.cpp
    Engine/Resource/StbImage.cpp
    Engine/Resource/TextureLoader.cpp
    Engine/Resource/MeshLoader.cpp
    Engine/Input/InputManager.cpp
    Engine/Physics/Collision.cpp
    Engine/Physics/VehicleConfig.cpp
    Engine/Physics/RigidBody.cpp
    Engine/Physics/VehiclePhysics.cpp
)

set(ENGINE_HEADERS
    Engine/Math/MathConstants.h
    Engine/Math/Vector3.h
    Engine/Math/Matrix4x4.h
    Engine/Math/Quaternion.h
    Engine/Math/CatmullRomSpline.h
    Engine/Platform/Window.h
    Engine/Core/Timer.h
    Engine/Core/Application.h
    Engine/Core/LinearAllocator.h
    Engine/Core/ObjectPool.h
    Engine/Audio/AudioManager.h
    Engine/Renderer/GraphicsDevice.h
    Engine/Renderer/VulkanHelpers.h
    Engine/Renderer/Vertex.h
    Engine/Renderer/ShaderManager.h
    Engine/Renderer/PipelineBuilder.h
    Engine/Renderer/DescriptorSetManager.h
    Engine/Renderer/MeshRenderer.h
    Engine/Renderer/Camera.h
    Engine/Renderer/DebugCameraController.h
    Engine/Renderer/Skybox.h
    Engine/Renderer/ShadowMap.h
    Engine/Renderer/ParticleRenderer.h
    Engine/Renderer/SpriteRenderer.h
    Engine/Resource/TextureLoader.h
    Engine/Resource/MeshLoader.h
    Engine/Input/InputManager.h
    Engine/Physics/Collision.h
    Engine/Physics/VehicleConfig.h
    Engine/Physics/RigidBody.h
    Engine/Physics/VehiclePhysics.h
)

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(Engine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${SDL2_INCLUDE_DIRS}
)

# stb ヘッダーのインクルードパス (Engine/Resource から "stb_image.h" でインクルード)
target_include_directories(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/stb"
)

# miniaudio ヘッダーのインクルードパス
# AudioManager.h が miniaudio.h を直接インクルードするため PUBLIC にする
target_include_directories(Engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/miniaudio"
)

target_link_libraries(Engine
    PUBLIC
        ${SDL2_LIBRARIES}
        Vulkan::Vulkan
        assimp::assimp
        nlohmann_json::nlohmann_json
)

# macOS: miniaudio が CoreAudio / AudioToolbox を使用する
if(APPLE)
    target_link_libraries(Engine PUBLIC
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

# Debug ビルドで Vulkan バリデーションレイヤーを有効化
target_compile_definitions(Engine PRIVATE
    $<$<CONFIG:Debug>:REVORA_ENABLE_VALIDATION>
)

# --- Game ソース ---
set(GAME_SOURCES
    Game/Main.cpp
    Game/Vehicle/VehicleInputMapper.cpp
    Game/Vehicle/VehicleController.cpp
    Game/Camera/ChaseCameraController.cpp
    Game/Course/CourseData.cpp
    Game/Course/CourseMeshGenerator.cpp
    Game/Course/CourseCollider.cpp
    Game/Course/CheckpointSystem.cpp
    Game/Course/LapTimer.cpp
    Game/Race/RaceManager.cpp
    Game/Race/GhostRecorder.cpp
    Game/Flow/GameFlowManager.cpp
    Game/Particle/ParticleEmitter.cpp
    Game/UI/BitmapFont.cpp
    Game/UI/HudOverlay.cpp
)

set(GAME_HEADERS
    Game/Vehicle/VehicleInputMapper.h
    Game/Vehicle/VehicleController.h
    Game/Camera/ChaseCameraController.h
    Game/Course/CourseData.h
    Game/Course/CourseMeshGenerator.h
    Game/Course/CourseCollider.h
    Game/Course/CheckpointSystem.h
    Game/Course/LapTimer.h
    Game/Race/RaceManager.h
    Game/Race/GhostRecorder.h
    Game/Flow/GameFlowManager.h
    Game/Particle/ParticleEmitter.h
    Game/UI/BitmapFont.h
    Game/UI/HudOverlay.h
)

# --- Game 実行バイナリ ---
add_executable(Revora ${GAME_SOURCES} ${GAME_HEADERS})

target_link_libraries(Revora PRIVATE Engine)
add_dependencies(Revora CompileShaders)

# main() エントリーポイントを使用 (SDL_main をバイパス)
target_compile_definitions(Revora PRIVATE SDL_MAIN_HANDLED)
target_compile_definitions(Engine PRIVATE SDL_MAIN_HANDLED)

# --- アセットとコンパイル済みシェーダーのコピー ---
add_custom_command(TARGET Revora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:Revora>/Assets"
    COMMENT "Copying Assets to output directory"
)

# SPIR-V ファイルを出力ディレクトリにコピー
add_custom_command(TARGET Revora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_OUTPUT_DIR}"
        "$<TARGET_FILE_DIR:Revora>/Assets/Shaders"
    COMMENT "Copying compiled SPIR-V shaders to output directory"
)

# --- Visual Studio フィルター (IDE上のフォルダ構成) ---
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Engine" PREFIX "Engine" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Game" PREFIX "Game" FILES ${GAME_SOURCES} ${GAME_HEADERS})
