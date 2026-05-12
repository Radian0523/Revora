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
                               DescriptorSetManager& descriptorMgr,
                               ShaderManager& shaderMgr)
{
    device_        = device;
    descriptorMgr_ = &descriptorMgr;

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

    // --- ディスクリプタセット確保 ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!descriptorMgr.AllocateDescriptorSet(descriptorSets_[i])) {
            return false;
        }
    }

    // --- パイプラインレイアウト ---
    // プッシュ定数: Model 行列 (64 bytes, 頂点シェーダー)
    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset     = 0;
    pushConstant.size       = sizeof(Matrix4x4);

    VkDescriptorSetLayout setLayout = descriptorMgr.GetLayout();

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &setLayout;
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

    // --- パイプライン生成 ---
    auto binding = Vertex::GetBindingDescription();
    auto attrs   = Vertex::GetAttributeDescriptions();

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule);
    builder.SetVertexInput(binding, attrs.data(), static_cast<uint32_t>(attrs.size()));
    builder.SetLayout(pipelineLayout_);
    builder.SetCullMode(VK_CULL_MODE_BACK_BIT);
    builder.SetDepthTest(true, true, VK_COMPARE_OP_LESS);

    pipeline_ = builder.Build(device, renderPass);
    return pipeline_ != VK_NULL_HANDLE;
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

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    device_ = VK_NULL_HANDLE;
}

void MeshRenderer::UpdateSceneData(uint32_t frameIndex,
                                    const Matrix4x4& view,
                                    const Matrix4x4& projection,
                                    const float lightDir[3])
{
    SceneUBO ubo;
    std::memcpy(ubo.view, &view, sizeof(float) * 16);
    std::memcpy(ubo.projection, &projection, sizeof(float) * 16);
    ubo.lightDirection[0] = lightDir[0];
    ubo.lightDirection[1] = lightDir[1];
    ubo.lightDirection[2] = lightDir[2];
    ubo.lightDirection[3] = 0.0f;

    std::memcpy(sceneUBOMapped_[frameIndex], &ubo, sizeof(SceneUBO));
}

void MeshRenderer::Draw(VkCommandBuffer cmd,
                         uint32_t frameIndex,
                         const MeshResource& mesh,
                         const TextureResource& texture,
                         const Matrix4x4& modelMatrix)
{
    // テクスチャが変わった場合のみディスクリプタセットを更新
    if (lastBoundTextureView_[frameIndex] != texture.view) {
        descriptorMgr_->UpdateDescriptorSet(
            descriptorSets_[frameIndex],
            sceneUBOBuffers_[frameIndex], sizeof(SceneUBO),
            texture.view, texture.sampler);
        lastBoundTextureView_[frameIndex] = texture.view;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    // プッシュ定数: Model 行列
    // C++ 行優先メモリ → GLSL 列優先解釈で暗黙転置
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(Matrix4x4), &modelMatrix);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
}

} // namespace Revora
