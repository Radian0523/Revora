#include "TextureLoader.h"
#include "../Renderer/VulkanHelpers.h"

#include <SDL.h>
#include "stb_image.h"

#include <cstring>

namespace Revora {

void TextureLoader::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkCommandPool commandPool, VkQueue graphicsQueue)
{
    device_         = device;
    physicalDevice_ = physicalDevice;
    commandPool_    = commandPool;
    graphicsQueue_  = graphicsQueue;
}

bool TextureLoader::LoadTexture2D(const std::string& filepath, TextureResource& outTexture) {
    // 実行ファイルのディレクトリを基準にパスを構築
    std::string fullPath;
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        fullPath = basePath;
        SDL_free(basePath);
    }
    fullPath += filepath;

    // stb_image で読み込み (RGBA 4チャンネル強制)
    int width, height, channels;
    stbi_uc* pixels = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to load texture: %s", fullPath.c_str());
        return false;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;

    // ステージングバッファにピクセルデータをコピー
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory)) {
        stbi_image_free(pixels);
        return false;
    }

    void* data;
    vkMapMemory(device_, stagingMemory, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device_, stagingMemory);

    stbi_image_free(pixels);

    // GPU イメージ生成
    if (!VulkanHelpers::CreateImage(
            device_, physicalDevice_,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height),
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outTexture.image, outTexture.memory)) {
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        return false;
    }

    // レイアウト遷移 → コピー → シェーダー読み取りへ遷移
    VkCommandBuffer cmd = VulkanHelpers::BeginOneTimeCommands(device_, commandPool_);
    VulkanHelpers::TransitionImageLayout(cmd, outTexture.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanHelpers::CopyBufferToImage(cmd, stagingBuffer, outTexture.image,
                                     static_cast<uint32_t>(width),
                                     static_cast<uint32_t>(height));
    VulkanHelpers::TransitionImageLayout(cmd, outTexture.image,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VulkanHelpers::EndOneTimeCommands(device_, commandPool_, graphicsQueue_, cmd);

    // ステージングバッファ解放
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    // イメージビュー・サンプラー生成
    outTexture.view = VulkanHelpers::CreateImageView(
        device_, outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    outTexture.sampler = VulkanHelpers::CreateSampler(device_);
    outTexture.width   = static_cast<uint32_t>(width);
    outTexture.height  = static_cast<uint32_t>(height);

    return outTexture.view != VK_NULL_HANDLE && outTexture.sampler != VK_NULL_HANDLE;
}

bool TextureLoader::LoadCubemap(const std::string faces[6], TextureResource& outTexture) {
    // 実行ファイルのベースパス取得
    std::string baseDir;
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        baseDir = basePath;
        SDL_free(basePath);
    }

    // 6面のテクスチャを読み込み、サイズを検証
    stbi_uc* facePixels[6] = {};
    int faceWidth = 0;
    int faceHeight = 0;

    for (int i = 0; i < 6; ++i) {
        std::string fullPath = baseDir + faces[i];
        int w, h, channels;
        facePixels[i] = stbi_load(fullPath.c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (!facePixels[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to load cubemap face: %s", fullPath.c_str());
            for (int j = 0; j < i; ++j) {
                stbi_image_free(facePixels[j]);
            }
            return false;
        }

        if (i == 0) {
            faceWidth  = w;
            faceHeight = h;
        } else if (w != faceWidth || h != faceHeight) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Cubemap face size mismatch: %s", fullPath.c_str());
            for (int j = 0; j <= i; ++j) {
                stbi_image_free(facePixels[j]);
            }
            return false;
        }
    }

    VkDeviceSize faceSize  = static_cast<VkDeviceSize>(faceWidth) * faceHeight * 4;
    VkDeviceSize totalSize = faceSize * 6;

    // ステージングバッファに全6面を連続配置
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, totalSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingMemory)) {
        for (int i = 0; i < 6; ++i) stbi_image_free(facePixels[i]);
        return false;
    }

    void* data;
    vkMapMemory(device_, stagingMemory, 0, totalSize, 0, &data);
    for (int i = 0; i < 6; ++i) {
        std::memcpy(static_cast<char*>(data) + faceSize * i,
                    facePixels[i], static_cast<size_t>(faceSize));
        stbi_image_free(facePixels[i]);
    }
    vkUnmapMemory(device_, stagingMemory);

    // キューブマップイメージ生成 (arrayLayers=6, CUBE_COMPATIBLE フラグ)
    if (!VulkanHelpers::CreateImage(
            device_, physicalDevice_,
            static_cast<uint32_t>(faceWidth), static_cast<uint32_t>(faceHeight),
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outTexture.image, outTexture.memory,
            6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)) {
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingMemory, nullptr);
        return false;
    }

    // レイアウト遷移 → 各面コピー → シェーダー読み取りへ遷移
    VkCommandBuffer cmd = VulkanHelpers::BeginOneTimeCommands(device_, commandPool_);

    VulkanHelpers::TransitionImageLayout(cmd, outTexture.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         6);

    // 6面分のコピーリージョンを一括指定
    VkBufferImageCopy regions[6] = {};
    for (uint32_t i = 0; i < 6; ++i) {
        regions[i].bufferOffset      = faceSize * i;
        regions[i].bufferRowLength   = 0;
        regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel       = 0;
        regions[i].imageSubresource.baseArrayLayer = i;
        regions[i].imageSubresource.layerCount     = 1;
        regions[i].imageOffset = {0, 0, 0};
        regions[i].imageExtent = {
            static_cast<uint32_t>(faceWidth),
            static_cast<uint32_t>(faceHeight),
            1
        };
    }
    vkCmdCopyBufferToImage(cmd, stagingBuffer, outTexture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           6, regions);

    VulkanHelpers::TransitionImageLayout(cmd, outTexture.image,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         6);

    VulkanHelpers::EndOneTimeCommands(device_, commandPool_, graphicsQueue_, cmd);

    // ステージングバッファ解放
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    // キューブマップビュー・サンプラー生成
    outTexture.view = VulkanHelpers::CreateImageView(
        device_, outTexture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE, 6);
    outTexture.sampler = VulkanHelpers::CreateSampler(
        device_, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    outTexture.width  = static_cast<uint32_t>(faceWidth);
    outTexture.height = static_cast<uint32_t>(faceHeight);

    return outTexture.view != VK_NULL_HANDLE && outTexture.sampler != VK_NULL_HANDLE;
}

void TextureLoader::DestroyTexture(TextureResource& texture) {
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
}

} // namespace Revora
