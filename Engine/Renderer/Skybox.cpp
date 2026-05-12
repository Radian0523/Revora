#include "Skybox.h"
#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include "VulkanHelpers.h"

#include <cstring>

namespace Revora {

/// スカイボックス用 UBO: View (平行移動除去済み) + Projection
struct SkyboxUBO {
    float view[16];
    float projection[16];
};

Skybox::~Skybox() {
    Shutdown();
}

bool Skybox::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkRenderPass renderPass,
                         DescriptorSetManager& descriptorMgr,
                         ShaderManager& shaderMgr)
{
    device_        = device;
    descriptorMgr_ = &descriptorMgr;

    // --- UBO バッファ生成 ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, sizeof(SkyboxUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                sceneUBOBuffers_[i], sceneUBOMemories_[i])) {
            return false;
        }
        vkMapMemory(device, sceneUBOMemories_[i], 0, sizeof(SkyboxUBO), 0, &sceneUBOMapped_[i]);
    }

    // --- ディスクリプタセット ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!descriptorMgr.AllocateDescriptorSet(descriptorSets_[i])) {
            return false;
        }
    }

    // --- パイプラインレイアウト ---
    VkDescriptorSetLayout setLayout = descriptorMgr.GetLayout();

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &setLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        return false;
    }

    // --- シェーダー ---
    VkShaderModule vertModule = shaderMgr.GetShaderModule("Assets/Shaders/Skybox.vert.spv");
    VkShaderModule fragModule = shaderMgr.GetShaderModule("Assets/Shaders/Skybox.frag.spv");
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        return false;
    }

    // --- パイプライン ---
    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule);
    builder.SetNoVertexInput();
    builder.SetLayout(pipelineLayout_);
    // キューブ内側から描画するためフロントフェイスをカリング
    builder.SetCullMode(VK_CULL_MODE_FRONT_BIT);
    // デプステスト有効 (LESS_OR_EQUAL) だがデプス書き込み無効
    // 頂点シェーダーで gl_Position = pos.xyww としてデプス値を最大 (1.0) に固定
    builder.SetDepthTest(true, false, VK_COMPARE_OP_LESS_OR_EQUAL);

    pipeline_ = builder.Build(device, renderPass);
    return pipeline_ != VK_NULL_HANDLE;
}

void Skybox::Shutdown() {
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
        lastBoundCubemapView_[i] = VK_NULL_HANDLE;
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

void Skybox::UpdateSceneData(uint32_t frameIndex,
                              const Matrix4x4& view,
                              const Matrix4x4& projection)
{
    // ビュー行列の平行移動成分を除去
    // カメラが移動してもスカイボックスは無限遠に見える
    Matrix4x4 viewNoTranslation = view;
    viewNoTranslation.m[3][0] = 0.0f;
    viewNoTranslation.m[3][1] = 0.0f;
    viewNoTranslation.m[3][2] = 0.0f;

    SkyboxUBO ubo;
    std::memcpy(ubo.view, &viewNoTranslation, sizeof(float) * 16);
    std::memcpy(ubo.projection, &projection, sizeof(float) * 16);

    std::memcpy(sceneUBOMapped_[frameIndex], &ubo, sizeof(SkyboxUBO));
}

void Skybox::Draw(VkCommandBuffer cmd,
                   uint32_t frameIndex,
                   const TextureResource& cubemap)
{
    if (lastBoundCubemapView_[frameIndex] != cubemap.view) {
        descriptorMgr_->UpdateDescriptorSet(
            descriptorSets_[frameIndex],
            sceneUBOBuffers_[frameIndex], sizeof(SkyboxUBO),
            cubemap.view, cubemap.sampler);
        lastBoundCubemapView_[frameIndex] = cubemap.view;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    // 36 頂点 = キューブ 12 三角形 (頂点シェーダー内で生成)
    vkCmdDraw(cmd, 36, 1, 0, 0);
}

} // namespace Revora
