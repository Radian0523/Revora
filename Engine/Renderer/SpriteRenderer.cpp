#include "SpriteRenderer.h"
#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include "VulkanHelpers.h"

#include <cstring>

namespace Revora {

/// 正射影 UBO 構造体 (シェーダーの ProjectionUBO に対応)
struct SpriteProjectionUBO {
    float projection[16];
};

SpriteRenderer::~SpriteRenderer()
{
    Shutdown();
}

bool SpriteRenderer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkRenderPass renderPass,
                                 ShaderManager& shaderMgr,
                                 uint32_t maxVertices)
{
    device_         = device;
    physicalDevice_ = physicalDevice;
    maxVertices_    = maxVertices;

    // --- 正射影 UBO ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, sizeof(SpriteProjectionUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                projUBOBuffers_[i], projUBOMemories_[i]))
        {
            return false;
        }
        vkMapMemory(device, projUBOMemories_[i], 0,
                     sizeof(SpriteProjectionUBO), 0, &projUBOMapped_[i]);
    }

    // --- 頂点バッファ ---
    VkDeviceSize bufferSize = sizeof(SpriteVertex) * maxVertices;
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffers_[i], vertexMemories_[i]))
        {
            return false;
        }
        vkMapMemory(device, vertexMemories_[i], 0, bufferSize, 0,
                     &vertexMapped_[i]);
        vertexCount_[i] = 0;
    }

    // --- ディスクリプタ ---
    if (!CreateDescriptorResources())
    {
        return false;
    }

    // --- パイプラインレイアウト ---
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &descriptorLayout_;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
    {
        return false;
    }

    // --- シェーダー ---
    VkShaderModule vertModule = shaderMgr.GetShaderModule("Assets/Shaders/Sprite.vert.spv");
    VkShaderModule fragModule = shaderMgr.GetShaderModule("Assets/Shaders/Sprite.frag.spv");
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
    {
        return false;
    }

    // --- パイプライン ---
    // 頂点入力: SpriteVertex (per-vertex, not per-instance)
    VkVertexInputBindingDescription vertexBinding = {};
    vertexBinding.binding   = 0;
    vertexBinding.stride    = sizeof(SpriteVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3] = {};
    // position: vec2
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset   = offsetof(SpriteVertex, position);
    // texCoord: vec2
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset   = offsetof(SpriteVertex, texCoord);
    // color: vec4
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[2].offset   = offsetof(SpriteVertex, color);

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule);
    builder.SetVertexInput(vertexBinding, attrs, 3);
    builder.SetLayout(pipelineLayout_);
    builder.SetCullMode(VK_CULL_MODE_NONE);
    builder.SetDepthTest(false, false);
    builder.SetBlendModeAlpha();

    pipeline_ = builder.Build(device, renderPass);
    return pipeline_ != VK_NULL_HANDLE;
}

void SpriteRenderer::Shutdown()
{
    if (device_ == VK_NULL_HANDLE)
    {
        return;
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        if (projUBOMapped_[i])
        {
            vkUnmapMemory(device_, projUBOMemories_[i]);
            projUBOMapped_[i] = nullptr;
        }
        if (projUBOBuffers_[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device_, projUBOBuffers_[i], nullptr);
            projUBOBuffers_[i] = VK_NULL_HANDLE;
        }
        if (projUBOMemories_[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device_, projUBOMemories_[i], nullptr);
            projUBOMemories_[i] = VK_NULL_HANDLE;
        }

        if (vertexMapped_[i])
        {
            vkUnmapMemory(device_, vertexMemories_[i]);
            vertexMapped_[i] = nullptr;
        }
        if (vertexBuffers_[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device_, vertexBuffers_[i], nullptr);
            vertexBuffers_[i] = VK_NULL_HANDLE;
        }
        if (vertexMemories_[i] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device_, vertexMemories_[i], nullptr);
            vertexMemories_[i] = VK_NULL_HANDLE;
        }
    }

    if (pipeline_ != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorLayout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
        descriptorLayout_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

void SpriteRenderer::UpdateProjection(uint32_t frameIndex,
                                       float screenWidth, float screenHeight)
{
    // (0,0)=左上, (screenWidth, screenHeight)=右下 を NDC にマッピング
    // GraphicsDevice が viewport.height を負にして Y 軸を反転しているため、
    // bottom と top を入れ替えて打ち消す
    Matrix4x4 proj = Matrix4x4::Orthographic(
        0.0f, screenWidth, screenHeight, 0.0f, -1.0f, 1.0f);

    SpriteProjectionUBO ubo;
    std::memcpy(ubo.projection, &proj, sizeof(float) * 16);
    std::memcpy(projUBOMapped_[frameIndex], &ubo, sizeof(SpriteProjectionUBO));
}

void SpriteRenderer::UploadVertices(uint32_t frameIndex,
                                     const void* vertices, uint32_t vertexCount)
{
    uint32_t uploadCount = (vertexCount < maxVertices_) ? vertexCount : maxVertices_;
    vertexCount_[frameIndex] = uploadCount;

    if (uploadCount == 0)
    {
        return;
    }

    std::memcpy(vertexMapped_[frameIndex], vertices,
                sizeof(SpriteVertex) * uploadCount);
}

void SpriteRenderer::Draw(VkCommandBuffer cmd, uint32_t frameIndex,
                           const TextureResource& texture)
{
    if (vertexCount_[frameIndex] == 0)
    {
        return;
    }

    // テクスチャが変更された場合のみディスクリプタセットを更新する
    if (lastBoundTextureView_[frameIndex] != texture.view)
    {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = texture.view;
        imageInfo.sampler     = texture.sampler;

        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSets_[frameIndex];
        write.dstBinding      = 1;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        lastBoundTextureView_[frameIndex] = texture.view;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffers_[frameIndex], &offset);

    vkCmdDraw(cmd, vertexCount_[frameIndex], 1, 0, 0);
}

bool SpriteRenderer::CreateDescriptorResources()
{
    // binding 0: 正射影行列 UBO (頂点シェーダー)
    // binding 1: フォントアトラスサンプラー (フラグメントシェーダー)
    VkDescriptorSetLayoutBinding bindings[2] = {};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings    = bindings;

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr,
                                     &descriptorLayout_) != VK_SUCCESS)
    {
        return false;
    }

    // プールサイズ: UBO × フレーム数 + サンプラー × フレーム数
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = kMaxFramesInFlight;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr,
                                &descriptorPool_) != VK_SUCCESS)
    {
        return false;
    }

    VkDescriptorSetLayout layouts[kMaxFramesInFlight];
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        layouts[i] = descriptorLayout_;
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts        = layouts;

    if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_) != VK_SUCCESS)
    {
        return false;
    }

    // UBO をディスクリプタセットに紐付け
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = projUBOBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(SpriteProjectionUBO);

        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSets_[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    return true;
}

} // namespace Revora
