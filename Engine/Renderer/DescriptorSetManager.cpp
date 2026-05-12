#include "DescriptorSetManager.h"

namespace Revora {

DescriptorSetManager::~DescriptorSetManager() {
    Shutdown();
}

bool DescriptorSetManager::Initialize(VkDevice device, uint32_t maxSets) {
    device_ = device;

    // ディスクリプタセットレイアウト生成
    // binding 0: UBO (頂点シェーダー), binding 1: テクスチャサンプラー (フラグメントシェーダー)
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

    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout_) != VK_SUCCESS) {
        return false;
    }

    // ディスクリプタプール生成
    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = maxSets;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = maxSets;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = maxSets;

    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void DescriptorSetManager::Shutdown() {
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    device_ = VK_NULL_HANDLE;
}

bool DescriptorSetManager::AllocateDescriptorSet(VkDescriptorSet& outSet) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout_;

    return vkAllocateDescriptorSets(device_, &allocInfo, &outSet) == VK_SUCCESS;
}

void DescriptorSetManager::UpdateDescriptorSet(VkDescriptorSet set,
                                                VkBuffer uniformBuffer, VkDeviceSize uniformSize,
                                                VkImageView textureView, VkSampler textureSampler)
{
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = uniformSize;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = textureView;
    imageInfo.sampler     = textureSampler;

    VkWriteDescriptorSet writes[2] = {};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = set;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo     = &bufferInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = set;
    writes[1].dstBinding      = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(device_, 2, writes, 0, nullptr);
}

} // namespace Revora
