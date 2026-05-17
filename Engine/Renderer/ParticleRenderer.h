#pragma once

#include "../Math/Matrix4x4.h"
#include "../Math/Vector3.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;
struct Particle;

/// パーティクルの GPU 描画を担当する Engine 層レンダラー
/// ビルボードクアッドをインスタンス描画し、加算ブレンドで発光表現を行う
///
/// 頂点シェーダーで gl_VertexIndex からクアッド 4 頂点を展開する方式を採用。
/// ジオメトリシェーダーを使わないことで MoltenVK 互換性を確保している。
class ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkRenderPass renderPass,
                    ShaderManager& shaderMgr,
                    uint32_t maxParticles);
    void Shutdown();

    /// カメラ行列を更新する
    void UpdateSceneData(uint32_t frameIndex,
                         const Matrix4x4& view,
                         const Matrix4x4& projection);

    /// パーティクルデータを GPU バッファに転送する
    void UploadParticles(uint32_t frameIndex,
                         const Particle* particles,
                         uint32_t count);

    /// 全パーティクルを一括描画する
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex);

private:
    /// パーティクルの頂点バッファレイアウト (インスタンスデータ)
    struct ParticleVertex {
        float position[3];  // ワールド座標
        float size;         // パーティクルサイズ
        float alpha;        // 透明度
        float color[3];     // RGB カラー
    };

    bool CreateDescriptorResources();

    static constexpr uint32_t kMaxFramesInFlight = 2;

    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // ディスクリプタ (カメラ UBO)
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_   = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSets_[kMaxFramesInFlight] = {};

    // UBO (カメラ行列)
    VkBuffer       cameraUBOBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory cameraUBOMemories_[kMaxFramesInFlight] = {};
    void*          cameraUBOMapped_[kMaxFramesInFlight] = {};

    // パーティクル頂点バッファ (インスタンスデータ、フレームごとにダブルバッファリング)
    VkBuffer       particleBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory particleMemories_[kMaxFramesInFlight] = {};
    void*          particleMapped_[kMaxFramesInFlight] = {};

    uint32_t particleCount_[kMaxFramesInFlight] = {};
    uint32_t maxParticles_ = 0;
};

} // namespace Revora
