#pragma once

#include "../Math/Matrix4x4.h"
#include "../Resource/TextureLoader.h"
#include "DescriptorSetManager.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;

/// キューブマップスカイボックスの描画
/// 頂点データは頂点シェーダー内で生成するため、頂点バッファ不要
/// デプス値を最大に固定し、全オブジェクトの背後に描画する
class Skybox {
public:
    Skybox() = default;
    ~Skybox();

    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkRenderPass renderPass,
                    DescriptorSetManager& descriptorMgr,
                    ShaderManager& shaderMgr);
    void Shutdown();

    /// フレーム開始時にシーンデータを更新する
    /// ビュー行列の平行移動成分を除去してカメラ移動に追従させない
    void UpdateSceneData(uint32_t frameIndex,
                         const Matrix4x4& view,
                         const Matrix4x4& projection);

    /// キューブマップテクスチャをバインドして描画する
    void Draw(VkCommandBuffer cmd,
              uint32_t frameIndex,
              const TextureResource& cubemap);

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;

    VkDevice device_ = VK_NULL_HANDLE;

    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // UBO (フレームごとにダブルバッファリング)
    VkBuffer       sceneUBOBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory sceneUBOMemories_[kMaxFramesInFlight] = {};
    void*          sceneUBOMapped_[kMaxFramesInFlight] = {};

    VkDescriptorSet descriptorSets_[kMaxFramesInFlight] = {};
    DescriptorSetManager* descriptorMgr_ = nullptr;

    VkImageView lastBoundCubemapView_[kMaxFramesInFlight] = {};
};

} // namespace Revora
