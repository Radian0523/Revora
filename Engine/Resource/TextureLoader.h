#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Revora {

/// GPU テクスチャリソース
/// イメージ、メモリ、ビュー、サンプラーを一括管理する
struct TextureResource {
    VkImage        image   = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
    uint32_t       width   = 0;
    uint32_t       height  = 0;
};

/// テクスチャローダー
/// stb_image でファイルを読み込み、ステージングバッファ経由で GPU にアップロードする
class TextureLoader {
public:
    TextureLoader() = default;

    TextureLoader(const TextureLoader&) = delete;
    TextureLoader& operator=(const TextureLoader&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue);

    /// 2D テクスチャを読み込む
    bool LoadTexture2D(const std::string& filepath, TextureResource& outTexture);

    /// キューブマップテクスチャを読み込む (6面分のファイルパスを指定)
    /// faces の順序: +X, -X, +Y, -Y, +Z, -Z
    bool LoadCubemap(const std::string faces[6], TextureResource& outTexture);

    /// テクスチャリソースを解放する
    void DestroyTexture(TextureResource& texture);

private:
    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkCommandPool    commandPool_    = VK_NULL_HANDLE;
    VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
};

} // namespace Revora
