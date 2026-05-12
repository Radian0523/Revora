#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

/// Vulkan リソース生成のヘルパー関数群
/// バッファ・イメージの生成、ステージング転送、レイアウト遷移など
/// 個々の関数は VkDevice / VkPhysicalDevice を引数に取り、状態を持たない
namespace VulkanHelpers {

/// 要求プロパティを満たすメモリタイプインデックスを検索する
/// 見つからなかった場合は UINT32_MAX を返す
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

/// バッファとバインド済みメモリを一括生成する
bool CreateBuffer(VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& outBuffer,
                  VkDeviceMemory& outMemory);

/// イメージとバインド済みメモリを一括生成する
bool CreateImage(VkDevice device,
                 VkPhysicalDevice physicalDevice,
                 uint32_t width,
                 uint32_t height,
                 VkFormat format,
                 VkImageTiling tiling,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& outImage,
                 VkDeviceMemory& outMemory,
                 uint32_t arrayLayers = 1,
                 VkImageCreateFlags flags = 0);

/// イメージビューを生成する
VkImageView CreateImageView(VkDevice device,
                             VkImage image,
                             VkFormat format,
                             VkImageAspectFlags aspectFlags,
                             VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                             uint32_t layerCount = 1);

/// 単発コマンドバッファの開始
VkCommandBuffer BeginOneTimeCommands(VkDevice device,
                                     VkCommandPool commandPool);

/// 単発コマンドバッファの終了と同期実行
void EndOneTimeCommands(VkDevice device,
                        VkCommandPool commandPool,
                        VkQueue queue,
                        VkCommandBuffer commandBuffer);

/// イメージレイアウトの遷移
void TransitionImageLayout(VkCommandBuffer cmd,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           uint32_t layerCount = 1);

/// バッファからイメージへのコピー
void CopyBufferToImage(VkCommandBuffer cmd,
                       VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height,
                       uint32_t layerCount = 1);

/// バッファ間コピー
void CopyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue queue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

/// サンプラーを生成する
VkSampler CreateSampler(VkDevice device,
                        VkFilter magFilter = VK_FILTER_LINEAR,
                        VkFilter minFilter = VK_FILTER_LINEAR,
                        VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

} // namespace VulkanHelpers
} // namespace Revora
