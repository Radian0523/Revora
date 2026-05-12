#include "MeshLoader.h"
#include "../Renderer/VulkanHelpers.h"

#include <SDL.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cstring>

namespace Revora {

void MeshLoader::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool, VkQueue graphicsQueue)
{
    device_         = device;
    physicalDevice_ = physicalDevice;
    commandPool_    = commandPool;
    graphicsQueue_  = graphicsQueue;
}

bool MeshLoader::LoadMesh(const std::string& filepath, MeshResource& outMesh) {
    // 実行ファイルのディレクトリを基準にパスを構築
    std::string fullPath;
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        fullPath = basePath;
        SDL_free(basePath);
    }
    fullPath += filepath;

    Assimp::Importer importer;

    // 三角形化、法線生成、UV 反転 (Vulkan の V 座標系に合わせる)
    const aiScene* scene = importer.ReadFile(fullPath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Assimp error: %s", importer.GetErrorString());
        return false;
    }

    // 全メッシュの頂点・インデックスを結合
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex v = {};
            v.position[0] = mesh->mVertices[i].x;
            v.position[1] = mesh->mVertices[i].y;
            v.position[2] = mesh->mVertices[i].z;

            if (mesh->HasNormals()) {
                v.normal[0] = mesh->mNormals[i].x;
                v.normal[1] = mesh->mNormals[i].y;
                v.normal[2] = mesh->mNormals[i].z;
            }

            if (mesh->mTextureCoords[0]) {
                v.texCoord[0] = mesh->mTextureCoords[0][i].x;
                v.texCoord[1] = mesh->mTextureCoords[0][i].y;
            }

            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(baseVertex + face.mIndices[j]);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Mesh has no vertices or indices: %s", fullPath.c_str());
        return false;
    }

    return UploadToGPU(vertices, indices, outMesh);
}

bool MeshLoader::CreateMesh(const std::vector<Vertex>& vertices,
                             const std::vector<uint32_t>& indices,
                             MeshResource& outMesh)
{
    if (vertices.empty() || indices.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateMesh: empty vertices or indices");
        return false;
    }
    return UploadToGPU(vertices, indices, outMesh);
}

bool MeshLoader::UploadToGPU(const std::vector<Vertex>& vertices,
                              const std::vector<uint32_t>& indices,
                              MeshResource& outMesh)
{
    // --- 頂点バッファ ---
    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();

    VkBuffer vertexStaging;
    VkDeviceMemory vertexStagingMem;
    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, vertexSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vertexStaging, vertexStagingMem)) {
        return false;
    }

    void* data;
    vkMapMemory(device_, vertexStagingMem, 0, vertexSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(vertexSize));
    vkUnmapMemory(device_, vertexStagingMem);

    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, vertexSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outMesh.vertexBuffer, outMesh.vertexMemory)) {
        vkDestroyBuffer(device_, vertexStaging, nullptr);
        vkFreeMemory(device_, vertexStagingMem, nullptr);
        return false;
    }

    VulkanHelpers::CopyBuffer(device_, commandPool_, graphicsQueue_,
                              vertexStaging, outMesh.vertexBuffer, vertexSize);
    vkDestroyBuffer(device_, vertexStaging, nullptr);
    vkFreeMemory(device_, vertexStagingMem, nullptr);

    // --- インデックスバッファ ---
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();

    VkBuffer indexStaging;
    VkDeviceMemory indexStagingMem;
    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, indexSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            indexStaging, indexStagingMem)) {
        DestroyMesh(outMesh);
        return false;
    }

    vkMapMemory(device_, indexStagingMem, 0, indexSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(indexSize));
    vkUnmapMemory(device_, indexStagingMem);

    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, indexSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outMesh.indexBuffer, outMesh.indexMemory)) {
        vkDestroyBuffer(device_, indexStaging, nullptr);
        vkFreeMemory(device_, indexStagingMem, nullptr);
        DestroyMesh(outMesh);
        return false;
    }

    VulkanHelpers::CopyBuffer(device_, commandPool_, graphicsQueue_,
                              indexStaging, outMesh.indexBuffer, indexSize);
    vkDestroyBuffer(device_, indexStaging, nullptr);
    vkFreeMemory(device_, indexStagingMem, nullptr);

    outMesh.indexCount = static_cast<uint32_t>(indices.size());
    return true;
}

void MeshLoader::DestroyMesh(MeshResource& mesh) {
    if (mesh.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, mesh.indexBuffer, nullptr);
        mesh.indexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, mesh.indexMemory, nullptr);
        mesh.indexMemory = VK_NULL_HANDLE;
    }
    if (mesh.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, mesh.vertexBuffer, nullptr);
        mesh.vertexBuffer = VK_NULL_HANDLE;
    }
    if (mesh.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, mesh.vertexMemory, nullptr);
        mesh.vertexMemory = VK_NULL_HANDLE;
    }
    mesh.indexCount = 0;
}

} // namespace Revora
