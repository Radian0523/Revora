#pragma once

#include "../Math/Matrix4x4.h"
#include "../Resource/MeshLoader.h"
#include "../Resource/TextureLoader.h"
#include "DescriptorSetManager.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;

/// フレーム共通のシーンデータ (UBO binding 0)
/// View + Projection 行列に加え、Lambert ライティング用の光源方向を持つ
struct SceneUBO {
    float view[16];
    float projection[16];
    float lightDirection[4];  // vec3 + padding
};

/// テクスチャ付きメッシュの描画を担当するレンダラー
/// パイプライン、UBO、ディスクリプタセットの管理を行う
class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkRenderPass renderPass,
                    DescriptorSetManager& descriptorMgr,
                    ShaderManager& shaderMgr);
    void Shutdown();

    /// フレーム開始時にシーンデータを更新する
    void UpdateSceneData(uint32_t frameIndex,
                         const Matrix4x4& view,
                         const Matrix4x4& projection,
                         const float lightDir[3]);

    /// メッシュを描画する (コマンドバッファは呼び出し元が管理)
    void Draw(VkCommandBuffer cmd,
              uint32_t frameIndex,
              const MeshResource& mesh,
              const TextureResource& texture,
              const Matrix4x4& modelMatrix);

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;

    VkDevice         device_ = VK_NULL_HANDLE;

    // パイプライン
    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // UBO (フレームごとにダブルバッファリング)
    VkBuffer       sceneUBOBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory sceneUBOMemories_[kMaxFramesInFlight] = {};
    void*          sceneUBOMapped_[kMaxFramesInFlight] = {};

    // ディスクリプタセット (テクスチャ差し替え時に更新)
    VkDescriptorSet descriptorSets_[kMaxFramesInFlight] = {};

    DescriptorSetManager* descriptorMgr_ = nullptr;

    // 現在バインド中のテクスチャを追跡し、変更時のみ更新する
    VkImageView lastBoundTextureView_[kMaxFramesInFlight] = {};
};

} // namespace Revora
