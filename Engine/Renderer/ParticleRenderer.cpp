#include "ParticleRenderer.h"
#include "PipelineBuilder.h"
#include "ShaderManager.h"
#include "VulkanHelpers.h"
#include "../../Game/Particle/ParticleEmitter.h"

#include <cstring>

namespace Revora {

/// カメラ UBO 構造体
struct ParticleCameraUBO {
    float view[16];
    float projection[16];
};

ParticleRenderer::~ParticleRenderer() {
    Shutdown();
}

bool ParticleRenderer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                                    VkRenderPass renderPass,
                                    ShaderManager& shaderMgr,
                                    uint32_t maxParticles)
{
    device_         = device;
    physicalDevice_ = physicalDevice;
    maxParticles_   = maxParticles;

    // --- カメラ UBO ---
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, sizeof(ParticleCameraUBO),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                cameraUBOBuffers_[i], cameraUBOMemories_[i])) {
            return false;
        }
        vkMapMemory(device, cameraUBOMemories_[i], 0, sizeof(ParticleCameraUBO), 0,
                     &cameraUBOMapped_[i]);
    }

    // --- パーティクル頂点バッファ (インスタンスデータ) ---
    VkDeviceSize bufferSize = sizeof(ParticleVertex) * maxParticles;
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (!VulkanHelpers::CreateBuffer(
                device, physicalDevice, bufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                particleBuffers_[i], particleMemories_[i])) {
            return false;
        }
        vkMapMemory(device, particleMemories_[i], 0, bufferSize, 0,
                     &particleMapped_[i]);
        particleCount_[i] = 0;
    }

    // --- ディスクリプタ ---
    if (!CreateDescriptorResources()) {
        return false;
    }

    // --- パイプラインレイアウト ---
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &descriptorLayout_;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        return false;
    }

    // --- シェーダー ---
    VkShaderModule vertModule = shaderMgr.GetShaderModule("Assets/Shaders/Particle.vert.spv");
    VkShaderModule fragModule = shaderMgr.GetShaderModule("Assets/Shaders/Particle.frag.spv");
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        return false;
    }

    // --- パイプライン ---
    // インスタンス頂点入力: パーティクルごとのデータ
    VkVertexInputBindingDescription vertexBinding = {};
    vertexBinding.binding   = 0;
    vertexBinding.stride    = sizeof(ParticleVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription attrs[4] = {};
    // position: vec3
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(ParticleVertex, position);
    // size: float
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32_SFLOAT;
    attrs[1].offset   = offsetof(ParticleVertex, size);
    // alpha: float
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32_SFLOAT;
    attrs[2].offset   = offsetof(ParticleVertex, alpha);
    // color: vec3
    attrs[3].binding  = 0;
    attrs[3].location = 3;
    attrs[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[3].offset   = offsetof(ParticleVertex, color);

    PipelineBuilder builder;
    builder.SetShaders(vertModule, fragModule);
    builder.SetVertexInput(vertexBinding, attrs, 4);
    builder.SetLayout(pipelineLayout_);
    builder.SetCullMode(VK_CULL_MODE_NONE);
    // 深度テスト有効 (他のオブジェクトの背後に隠れる) だが深度書き込み無効
    builder.SetDepthTest(true, false, VK_COMPARE_OP_LESS);
    builder.SetBlendModeAdditive();

    pipeline_ = builder.Build(device, renderPass);
    return pipeline_ != VK_NULL_HANDLE;
}

void ParticleRenderer::Shutdown()
{
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (cameraUBOMapped_[i]) {
            vkUnmapMemory(device_, cameraUBOMemories_[i]);
            cameraUBOMapped_[i] = nullptr;
        }
        if (cameraUBOBuffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, cameraUBOBuffers_[i], nullptr);
            cameraUBOBuffers_[i] = VK_NULL_HANDLE;
        }
        if (cameraUBOMemories_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device_, cameraUBOMemories_[i], nullptr);
            cameraUBOMemories_[i] = VK_NULL_HANDLE;
        }

        if (particleMapped_[i]) {
            vkUnmapMemory(device_, particleMemories_[i]);
            particleMapped_[i] = nullptr;
        }
        if (particleBuffers_[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, particleBuffers_[i], nullptr);
            particleBuffers_[i] = VK_NULL_HANDLE;
        }
        if (particleMemories_[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device_, particleMemories_[i], nullptr);
            particleMemories_[i] = VK_NULL_HANDLE;
        }
    }

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
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

void ParticleRenderer::UpdateSceneData(uint32_t frameIndex,
                                        const Matrix4x4& view,
                                        const Matrix4x4& projection)
{
    ParticleCameraUBO ubo;
    std::memcpy(ubo.view, &view, sizeof(float) * 16);
    std::memcpy(ubo.projection, &projection, sizeof(float) * 16);
    std::memcpy(cameraUBOMapped_[frameIndex], &ubo, sizeof(ParticleCameraUBO));
}

void ParticleRenderer::UploadParticles(uint32_t frameIndex,
                                        const Particle* particles,
                                        uint32_t count)
{
    uint32_t uploadCount = (count < maxParticles_) ? count : maxParticles_;
    particleCount_[frameIndex] = uploadCount;

    if (uploadCount == 0) {
        return;
    }

    // Particle 構造体から ParticleVertex に変換して GPU バッファに書き込む
    auto* dst = static_cast<ParticleVertex*>(particleMapped_[frameIndex]);
    for (uint32_t i = 0; i < uploadCount; ++i) {
        const Particle& src = particles[i];
        dst[i].position[0] = src.position.x;
        dst[i].position[1] = src.position.y;
        dst[i].position[2] = src.position.z;
        dst[i].size         = src.size;
        dst[i].alpha        = src.alpha;
        dst[i].color[0]     = src.color.x;
        dst[i].color[1]     = src.color.y;
        dst[i].color[2]     = src.color.z;
    }
}

void ParticleRenderer::Draw(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (particleCount_[frameIndex] == 0) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &particleBuffers_[frameIndex], &offset);

    // 6 頂点/パーティクル (クアッド 2 三角形) × インスタンス数
    vkCmdDraw(cmd, 6, particleCount_[frameIndex], 0, 0);
}

bool ParticleRenderer::CreateDescriptorResources()
{
    // binding 0: カメラ UBO
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

    VkDescriptorPoolSize poolSize = {};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = kMaxFramesInFlight;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayout layouts[kMaxFramesInFlight];
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        layouts[i] = descriptorLayout_;
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool_;
    allocInfo.descriptorSetCount = kMaxFramesInFlight;
    allocInfo.pSetLayouts        = layouts;

    if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_) != VK_SUCCESS) {
        return false;
    }

    // UBO をディスクリプタセットに紐付け
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = cameraUBOBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(ParticleCameraUBO);

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
