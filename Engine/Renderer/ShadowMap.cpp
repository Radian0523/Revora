#include "ShadowMap.h"
#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include "Vertex.h"
#include "VulkanHelpers.h"

#include <cstring>

namespace Revora {

/// ライト VP 行列を UBO として渡す構造体
struct LightUBO {
    float lightVP[16];
};

ShadowMap::~ShadowMap() {
    Shutdown();
}

bool ShadowMap::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                            ShaderManager& shaderMgr)
{
    device_         = device;
    physicalDevice_ = physicalDevice;

    if (!CreateDepthResources()) { return false; }
    if (!CreateRenderPass())     { return false; }
    if (!CreateFramebuffer())    { return false; }
    if (!CreateDescriptorResources()) { return false; }
    if (!CreatePipeline(shaderMgr))   { return false; }

    return true;
}

void ShadowMap::Shutdown() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    if (lightUBOMapped_) {
        vkUnmapMemory(device_, lightUBOMemory_);
        lightUBOMapped_ = nullptr;
    }
    if (lightUBOBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, lightUBOBuffer_, nullptr);
        lightUBOBuffer_ = VK_NULL_HANDLE;
    }
    if (lightUBOMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, lightUBOMemory_, nullptr);
        lightUBOMemory_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (descriptorLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
        descriptorLayout_ = VK_NULL_HANDLE;
    }
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthMemory_, nullptr);
        depthMemory_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

void ShadowMap::UpdateLightMatrix(const Vector3& lightDir, const Vector3& sceneCenter)
{
    // ライト位置: シーン中心からライト方向の逆方向に一定距離
    Vector3 dir = lightDir.Normalized();
    Vector3 lightPos = sceneCenter + dir * kLightDistance;

    // ライトのビュー行列 (ライト位置からシーン中心を見る)
    Matrix4x4 lightView = Matrix4x4::LookAtLH(lightPos, sceneCenter, Vector3::Up);

    // 正射影: コース全体をカバーする固定範囲
    // 左手座標系の正射影行列を直接構築
    float l = -kOrthoHalfExtent, r = kOrthoHalfExtent;
    float b = -kOrthoHalfExtent, t = kOrthoHalfExtent;
    float n = 0.1f, f = kLightDistance * 2.0f;

    Matrix4x4 lightProj = {};
    lightProj.m[0][0] = 2.0f / (r - l);
    lightProj.m[1][1] = 2.0f / (t - b);
    lightProj.m[2][2] = 1.0f / (f - n);
    lightProj.m[3][2] = -n / (f - n);
    lightProj.m[3][3] = 1.0f;

    lightVP_ = lightView * lightProj;

    // UBO を更新
    LightUBO ubo;
    std::memcpy(ubo.lightVP, &lightVP_, sizeof(float) * 16);
    std::memcpy(lightUBOMapped_, &ubo, sizeof(LightUBO));
}

void ShadowMap::BeginPass(VkCommandBuffer cmd)
{
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass  = renderPass_;
    renderPassInfo.framebuffer = framebuffer_;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {kShadowMapSize, kShadowMapSize};

    // 深度クリア値: 1.0 (最遠)
    VkClearValue clearValue = {};
    clearValue.depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearValue;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ビューポートとシザーを設定
    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(kShadowMapSize);
    viewport.height   = static_cast<float>(kShadowMapSize);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {kShadowMapSize, kShadowMapSize};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // パイプラインとディスクリプタをバインド
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSet_, 0, nullptr);
}

void ShadowMap::EndPass(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

void ShadowMap::DrawMesh(VkCommandBuffer cmd,
                          const MeshResource& mesh,
                          const Matrix4x4& modelMatrix)
{
    // プッシュ定数: Model 行列 (シャドウ深度シェーダーと互換)
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(Matrix4x4), &modelMatrix);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
}

bool ShadowMap::CreateDepthResources()
{
    // D32_SFLOAT 深度テクスチャ
    if (!VulkanHelpers::CreateImage(
            device_, physicalDevice_,
            kShadowMapSize, kShadowMapSize,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImage_, depthMemory_)) {
        return false;
    }

    depthImageView_ = VulkanHelpers::CreateImageView(
        device_, depthImage_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (depthImageView_ == VK_NULL_HANDLE) {
        return false;
    }

    // シャドウサンプリング用サンプラー
    // border color を白 (1.0) にすることで、シャドウマップ範囲外は影なしになる
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter     = VK_FILTER_LINEAR;
    samplerInfo.minFilter     = VK_FILTER_LINEAR;
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    return vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) == VK_SUCCESS;
}

bool ShadowMap::CreateRenderPass()
{
    // 深度アタッチメントのみのレンダーパス
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    // サブパス依存: メインパスからの読み取りと同期
    VkSubpassDependency dependencies[2] = {};

    // 前のフレームのメインパスのフラグメント読み取り → シャドウパスの深度書き込み
    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // シャドウパスの深度書き込み → メインパスのフラグメント読み取り
    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &depthAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies   = dependencies;

    return vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) == VK_SUCCESS;
}

bool ShadowMap::CreateFramebuffer()
{
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = renderPass_;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &depthImageView_;
    fbInfo.width           = kShadowMapSize;
    fbInfo.height          = kShadowMapSize;
    fbInfo.layers          = 1;

    return vkCreateFramebuffer(device_, &fbInfo, nullptr, &framebuffer_) == VK_SUCCESS;
}

bool ShadowMap::CreateDescriptorResources()
{
    // ライト VP 行列用 UBO
    if (!VulkanHelpers::CreateBuffer(
            device_, physicalDevice_, sizeof(LightUBO),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            lightUBOBuffer_, lightUBOMemory_)) {
        return false;
    }
    vkMapMemory(device_, lightUBOMemory_, 0, sizeof(LightUBO), 0, &lightUBOMapped_);

    // ディスクリプタセットレイアウト: binding 0 = UBO (頂点シェーダー)
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding         = 0;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorLayout_) != VK_SUCCESS) {
        return false;
    }

    // ディスクリプタプール
    VkDescriptorPoolSize poolSize = {};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        return false;
    }

    // ディスクリプタセット確保
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &descriptorLayout_;

    if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        return false;
    }

    // UBO をディスクリプタセットに紐付け
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = lightUBOBuffer_;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(LightUBO);

    VkWriteDescriptorSet write = {};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = descriptorSet_;
    write.dstBinding      = 0;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    return true;
}

bool ShadowMap::CreatePipeline(ShaderManager& shaderMgr)
{
    // プッシュ定数: Model 行列 (64 bytes, 頂点シェーダー)
    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset     = 0;
    pushConstant.size       = sizeof(Matrix4x4);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &descriptorLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstant;

    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        return false;
    }

    VkShaderModule vertModule = shaderMgr.GetShaderModule("Assets/Shaders/ShadowDepth.vert.spv");
    VkShaderModule fragModule = shaderMgr.GetShaderModule("Assets/Shaders/ShadowDepth.frag.spv");
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        return false;
    }

    auto binding = Vertex::GetBindingDescription();
    auto attrs   = Vertex::GetAttributeDescriptions();

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule);
    builder.SetVertexInput(binding, attrs.data(), static_cast<uint32_t>(attrs.size()));
    builder.SetLayout(pipelineLayout_);
    builder.SetCullMode(VK_CULL_MODE_BACK_BIT);
    builder.SetDepthTest(true, true, VK_COMPARE_OP_LESS);

    // 深度バイアスで shadow acne を防止
    builder.SetDepthBias(true, 1.25f, 1.75f);

    pipeline_ = builder.Build(device_, renderPass_);
    return pipeline_ != VK_NULL_HANDLE;
}

} // namespace Revora
