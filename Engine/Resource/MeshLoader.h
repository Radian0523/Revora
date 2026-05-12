#pragma once

#include "../Renderer/Vertex.h"

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace Revora {

/// GPU メッシュリソース
/// 頂点バッファとインデックスバッファをデバイスローカルメモリに保持する
struct MeshResource {
    VkBuffer       vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer       indexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory  = VK_NULL_HANDLE;
    uint32_t       indexCount   = 0;
};

/// メッシュローダー
/// Assimp でモデルファイルを読み込み、ステージングバッファ経由で GPU にアップロードする
class MeshLoader {
public:
    MeshLoader() = default;

    MeshLoader(const MeshLoader&) = delete;
    MeshLoader& operator=(const MeshLoader&) = delete;

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkCommandPool commandPool, VkQueue graphicsQueue);

    /// モデルファイルを読み込み、GPU バッファを生成する
    bool LoadMesh(const std::string& filepath, MeshResource& outMesh);

    /// メッシュリソースを解放する
    void DestroyMesh(MeshResource& mesh);

private:
    /// CPU 側の頂点/インデックスデータをステージングバッファ経由で GPU にアップロードする
    bool UploadToGPU(const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices,
                     MeshResource& outMesh);

    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkCommandPool    commandPool_    = VK_NULL_HANDLE;
    VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
};

} // namespace Revora
