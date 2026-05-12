#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

/// ディスクリプタレイアウト・プール・セットの一括管理
/// メッシュ描画とスカイボックス描画で共通のレイアウト構造を使用する:
///   binding 0: UNIFORM_BUFFER (頂点シェーダー) — SceneUBO
///   binding 1: COMBINED_IMAGE_SAMPLER (フラグメントシェーダー) — テクスチャ
class DescriptorSetManager {
public:
    DescriptorSetManager() = default;
    ~DescriptorSetManager();

    DescriptorSetManager(const DescriptorSetManager&) = delete;
    DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;

    bool Initialize(VkDevice device, uint32_t maxSets);
    void Shutdown();

    VkDescriptorSetLayout GetLayout() const { return layout_; }

    /// ディスクリプタセットを確保する
    bool AllocateDescriptorSet(VkDescriptorSet& outSet);

    /// ディスクリプタセットを更新する (UBO + テクスチャ)
    void UpdateDescriptorSet(VkDescriptorSet set,
                             VkBuffer uniformBuffer, VkDeviceSize uniformSize,
                             VkImageView textureView, VkSampler textureSampler);

private:
    VkDevice              device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool      pool_   = VK_NULL_HANDLE;
};

} // namespace Revora
