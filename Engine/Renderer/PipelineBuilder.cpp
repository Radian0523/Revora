#include "PipelineBuilder.h"

#include <cstring>

namespace Revora {

PipelineBuilder::PipelineBuilder() {
    std::memset(shaderStages_, 0, sizeof(shaderStages_));
}

void PipelineBuilder::SetShaders(VkShaderModule vertModule, VkShaderModule fragModule) {
    shaderStages_[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages_[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages_[0].module = vertModule;
    shaderStages_[0].pName  = "main";

    shaderStages_[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages_[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages_[1].module = fragModule;
    shaderStages_[1].pName  = "main";
}

void PipelineBuilder::SetVertexInput(const VkVertexInputBindingDescription& binding,
                                     const VkVertexInputAttributeDescription* attributes,
                                     uint32_t attributeCount) {
    vertexBinding_ = binding;
    vertexAttributes_.assign(attributes, attributes + attributeCount);
    hasVertexInput_ = true;
}

void PipelineBuilder::SetNoVertexInput() {
    hasVertexInput_ = false;
    vertexAttributes_.clear();
}

void PipelineBuilder::SetLayout(VkPipelineLayout layout) {
    layout_ = layout;
}

void PipelineBuilder::SetCullMode(VkCullModeFlags cullMode) {
    cullMode_ = cullMode;
}

void PipelineBuilder::SetFrontFace(VkFrontFace frontFace) {
    frontFace_ = frontFace;
}

void PipelineBuilder::SetDepthTest(bool enable, bool writeEnable, VkCompareOp compareOp) {
    depthTestEnable_  = enable;
    depthWriteEnable_ = writeEnable;
    depthCompareOp_   = compareOp;
}

VkPipeline PipelineBuilder::Build(VkDevice device, VkRenderPass renderPass, uint32_t subpass) {
    // 頂点入力
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (hasVertexInput_) {
        vertexInputInfo.vertexBindingDescriptionCount   = 1;
        vertexInputInfo.pVertexBindingDescriptions      = &vertexBinding_;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes_.size());
        vertexInputInfo.pVertexAttributeDescriptions    = vertexAttributes_.data();
    }

    // 入力アセンブリ
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ビューポート・シザー (動的ステート)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ラスタライザ
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable  = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = cullMode_;
    rasterizer.frontFace               = frontFace_;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // マルチサンプリング (無効)
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // デプス・ステンシル
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = depthTestEnable_ ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable      = depthWriteEnable_ ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp        = depthCompareOp_;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // カラーブレンド (不透明)
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // 動的ステート (ビューポートとシザー)
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // パイプライン生成
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages_;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = layout_;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = subpass;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    return pipeline;
}

} // namespace Revora
