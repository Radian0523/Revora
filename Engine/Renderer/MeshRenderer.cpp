#include "MeshRenderer.h"
#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include "Vertex.h"
#include "VulkanHelpers.h"

#include <cstring>

namespace Revora {

MeshRenderer::~MeshRenderer() {
    Shutdown();
}

bool MeshRenderer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                               VkRenderPass renderPass,
                               ShaderManager& shaderMgr)
{
    device_         = device;
    physicalDevice_ = physicalDevice;

    // --- ダミーテクスチャ生成 (バインディング未設定時の安全な代替) ---
    if (!CreateDummyTextures()) {
        return false;
    }

    // --- UBO バッファ生成 (フレームごとにダブルバッファリング) ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, sizeof(SceneUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                sceneUBOBuffers_[i], sceneUBOMemories_[i])) {
            return false;
        }

        // パーシステントマッピング: 毎フレーム map/unmap するオーバーヘッドを回避
        vkMapMemory(device, sceneUBOMemories_[i], 0, sizeof(SceneUBO), 0, &sceneUBOMapped_[i]);
    }

    // --- 独自ディスクリプタリソース ---
    if (!CreateDescriptorResources()) {
        return false;
    }

    // --- パイプラインレイアウト ---
    // プッシュ定数: Model 行列 + alpha + reflectivity (80 bytes)
    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset     = 0;
    pushConstant.size       = sizeof(MeshPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &descriptorLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstant;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        return false;
    }

    // --- シェーダー読み込み ---
    VkShaderModule vertModule = shaderMgr.GetShaderModule("Assets/Shaders/Mesh.vert.spv");
    VkShaderModule fragModule = shaderMgr.GetShaderModule("Assets/Shaders/Mesh.frag.spv");
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        return false;
    }

    auto binding = Vertex::GetBindingDescription();
    auto attrs   = Vertex::GetAttributeDescriptions();

    // --- 不透明パイプライン ---
    {
        PipelineBuilder builder;
        builder.SetShaders(vertModule, fragModule);
        builder.SetVertexInput(binding, attrs.data(), static_cast<uint32_t>(attrs.size()));
        builder.SetLayout(pipelineLayout_);
        builder.SetCullMode(VK_CULL_MODE_BACK_BIT);
        builder.SetDepthTest(true, true, VK_COMPARE_OP_LESS);

        opaquePipeline_ = builder.Build(device, renderPass);
        if (opaquePipeline_ == VK_NULL_HANDLE) {
            return false;
        }
    }

    // --- 半透明パイプライン ---
    // アルファブレンド有効、深度テスト有効だが深度書き込み無効
    // 不透明オブジェクトの後に描画することで正しい重なり順を保証する
    {
        PipelineBuilder builder;
        builder.SetShaders(vertModule, fragModule);
        builder.SetVertexInput(binding, attrs.data(), static_cast<uint32_t>(attrs.size()));
        builder.SetLayout(pipelineLayout_);
        builder.SetCullMode(VK_CULL_MODE_BACK_BIT);
        builder.SetDepthTest(true, false, VK_COMPARE_OP_LESS);
        builder.SetBlendModeAlpha();

        transparentPipeline_ = builder.Build(device, renderPass);
        if (transparentPipeline_ == VK_NULL_HANDLE) {
            return false;
        }
    }

    return true;
}

void MeshRenderer::Shutdown() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (sceneUBOMapped_[i]) {
            vkUnmapMemory(device_, sceneUBOMemories_[i]);
            sceneUBOMapped_[i] = nullptr;
        }
        if (sceneUBOBuffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, sceneUBOBuffers_[i], nullptr);
            sceneUBOBuffers_[i] = VK_NULL_HANDLE;
        }
        if (sceneUBOMemories_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device_, sceneUBOMemories_[i], nullptr);
            sceneUBOMemories_[i] = VK_NULL_HANDLE;
        }
        lastBoundTextureView_[i] = VK_NULL_HANDLE;
    }

    // ダミーテクスチャ
    if (dummySampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, dummySampler_, nullptr);
        dummySampler_ = VK_NULL_HANDLE;
    }
    if (dummyView2D_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, dummyView2D_, nullptr);
        dummyView2D_ = VK_NULL_HANDLE;
    }
    if (dummyImage2D_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, dummyImage2D_, nullptr);
        dummyImage2D_ = VK_NULL_HANDLE;
    }
    if (dummyMemory2D_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, dummyMemory2D_, nullptr);
        dummyMemory2D_ = VK_NULL_HANDLE;
    }
    if (dummyCubeView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, dummyCubeView_, nullptr);
        dummyCubeView_ = VK_NULL_HANDLE;
    }
    if (dummyCubeImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, dummyCubeImage_, nullptr);
        dummyCubeImage_ = VK_NULL_HANDLE;
    }
    if (dummyCubeMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, dummyCubeMemory_, nullptr);
        dummyCubeMemory_ = VK_NULL_HANDLE;
    }

    // パイプライン
    if (opaquePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, opaquePipeline_, nullptr);
        opaquePipeline_ = VK_NULL_HANDLE;
    }
    if (transparentPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, transparentPipeline_, nullptr);
        transparentPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    // ディスクリプタ
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
        descriptorLayout_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

void MeshRenderer::SetShadowMap(VkImageView shadowView, VkSampler shadowSampler)
{
    shadowMapView_    = shadowView;
    shadowMapSampler_ = shadowSampler;

    // 全フレームのディスクリプタセットをリフレッシュ対象にする
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        lastBoundTextureView_[i] = VK_NULL_HANDLE;
    }
}

void MeshRenderer::SetEnvironmentMap(VkImageView envView, VkSampler envSampler)
{
    envMapView_    = envView;
    envMapSampler_ = envSampler;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        lastBoundTextureView_[i] = VK_NULL_HANDLE;
    }
}

void MeshRenderer::UpdateSceneData(uint32_t frameIndex,
                                    const Matrix4x4& view,
                                    const Matrix4x4& projection,
                                    const float lightDir[3],
                                    const Matrix4x4& lightVP,
                                    const float cameraPos[3])
{
    SceneUBO ubo;
    std::memcpy(ubo.view, &view, sizeof(float) * 16);
    std::memcpy(ubo.projection, &projection, sizeof(float) * 16);
    ubo.lightDirection[0] = lightDir[0];
    ubo.lightDirection[1] = lightDir[1];
    ubo.lightDirection[2] = lightDir[2];
    ubo.lightDirection[3] = 0.0f;
    std::memcpy(ubo.lightVP, &lightVP, sizeof(float) * 16);
    ubo.cameraPosition[0] = cameraPos[0];
    ubo.cameraPosition[1] = cameraPos[1];
    ubo.cameraPosition[2] = cameraPos[2];
    ubo.cameraPosition[3] = 0.0f;

    std::memcpy(sceneUBOMapped_[frameIndex], &ubo, sizeof(SceneUBO));
}

void MeshRenderer::Draw(VkCommandBuffer cmd,
                         uint32_t frameIndex,
                         const MeshResource& mesh,
                         const TextureResource& texture,
                         const Matrix4x4& modelMatrix,
                         float reflectivity)
{
    // テクスチャが変わった場合のみ全バインディングを更新
    if (lastBoundTextureView_[frameIndex] != texture.view) {
        UpdateAllDescriptorSets(frameIndex, texture);
        lastBoundTextureView_[frameIndex] = texture.view;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    // プッシュ定数: Model 行列 + alpha + reflectivity
    MeshPushConstants pc = {};
    pc.model        = modelMatrix;
    pc.alpha        = 1.0f;
    pc.reflectivity = reflectivity;
    pc.padding[0]   = 0.0f;
    pc.padding[1]   = 0.0f;

    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MeshPushConstants), &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
}

void MeshRenderer::DrawTransparent(VkCommandBuffer cmd,
                                    uint32_t frameIndex,
                                    const MeshResource& mesh,
                                    const TextureResource& texture,
                                    const Matrix4x4& modelMatrix,
                                    float alpha)
{
    if (lastBoundTextureView_[frameIndex] != texture.view) {
        UpdateAllDescriptorSets(frameIndex, texture);
        lastBoundTextureView_[frameIndex] = texture.view;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    MeshPushConstants pc = {};
    pc.model        = modelMatrix;
    pc.alpha        = alpha;
    pc.reflectivity = 0.0f;  // 半透明オブジェクトは反射なし
    pc.padding[0]   = 0.0f;
    pc.padding[1]   = 0.0f;

    vkCmdPushConstants(cmd, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MeshPushConstants), &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
}

bool MeshRenderer::CreateDescriptorResources()
{
    // 4 バインディングのレイアウト:
    //   0: UBO (vert+frag) — SceneUBO
    //   1: sampler2D (frag) — テクスチャ
    //   2: sampler2D (frag) — シャドウマップ
    //   3: samplerCube (frag) — 環境キューブマップ
    VkDescriptorSetLayoutBinding bindings[4] = {};

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding         = 2;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[3].binding         = 3;
    bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings    = bindings;

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorLayout_) != VK_SUCCESS) {
        return false;
    }

    // ディスクリプタプール: UBO x2 + サンプラー x6 (テクスチャ+シャドウ+環境 各2フレーム)
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = kMaxFramesInFlight;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = kMaxFramesInFlight * 3;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        return false;
    }

    // ディスクリプタセット確保
    VkDescriptorSetLayout layouts[kMaxFramesInFlight];
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        layouts[i] = descriptorLayout_;
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts        = layouts;

    return vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_) == VK_SUCCESS;
}

bool MeshRenderer::CreateDummyTextures()
{
    // 1x1 白色の 2D テクスチャ (シャドウマップ未設定時: 深度 1.0 = 影なし)
    if (!VulkanHelpers::CreateImage(
            device_, physicalDevice_, 1, 1,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dummyImage2D_, dummyMemory2D_)) {
        return false;
    }

    dummyView2D_ = VulkanHelpers::CreateImageView(
        device_, dummyImage2D_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    // 1x1 キューブマップ (環境マップ未設定時用)
    if (!VulkanHelpers::CreateImage(
            device_, physicalDevice_, 1, 1,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dummyCubeImage_, dummyCubeMemory_,
            6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)) {
        return false;
    }

    dummyCubeView_ = VulkanHelpers::CreateImageView(
        device_, dummyCubeImage_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE, 6);

    dummySampler_ = VulkanHelpers::CreateSampler(device_);

    return dummyView2D_ != VK_NULL_HANDLE
        && dummyCubeView_ != VK_NULL_HANDLE
        && dummySampler_ != VK_NULL_HANDLE;
}

void MeshRenderer::UpdateAllDescriptorSets(uint32_t frameIndex,
                                             const TextureResource& texture)
{
    // binding 0: UBO
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = sceneUBOBuffers_[frameIndex];
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(SceneUBO);

    // binding 1: テクスチャ
    VkDescriptorImageInfo textureImageInfo = {};
    textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textureImageInfo.imageView   = texture.view;
    textureImageInfo.sampler     = texture.sampler;

    // binding 2: シャドウマップ (未設定時はダミー)
    VkImageView shadowView    = shadowMapView_    ? shadowMapView_    : dummyView2D_;
    VkSampler   shadowSampler = shadowMapSampler_ ? shadowMapSampler_ : dummySampler_;

    VkDescriptorImageInfo shadowImageInfo = {};
    shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadowImageInfo.imageView   = shadowView;
    shadowImageInfo.sampler     = shadowSampler;

    // binding 3: 環境マップ (未設定時はダミー)
    VkImageView envView    = envMapView_    ? envMapView_    : dummyCubeView_;
    VkSampler   envSampler = envMapSampler_ ? envMapSampler_ : dummySampler_;

    VkDescriptorImageInfo envImageInfo = {};
    envImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envImageInfo.imageView   = envView;
    envImageInfo.sampler     = envSampler;

    VkWriteDescriptorSet writes[4] = {};

    // binding 0: UBO
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = descriptorSets_[frameIndex];
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo     = &bufferInfo;

    // binding 1: テクスチャ
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = descriptorSets_[frameIndex];
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &textureImageInfo;

    // binding 2: シャドウマップ
    writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet          = descriptorSets_[frameIndex];
    writes[2].dstBinding      = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo      = &shadowImageInfo;

    // binding 3: 環境マップ
    writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet          = descriptorSets_[frameIndex];
    writes[3].dstBinding      = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo      = &envImageInfo;

    vkUpdateDescriptorSets(device_, 4, writes, 0, nullptr);
}

} // namespace Revora
