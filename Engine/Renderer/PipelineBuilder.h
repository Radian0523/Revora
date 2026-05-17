#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Revora {

/// グラフィックスパイプライン生成ヘルパー
/// 合理的なデフォルト値を持ち、必要な部分だけカスタマイズして使う
/// Usage:
///   PipelineBuilder builder;
///   builder.SetShaders(vert, frag);
///   builder.SetVertexInput(binding, attrs);
///   builder.SetLayout(layout);
///   VkPipeline pipeline = builder.Build(device, renderPass);
class PipelineBuilder {
public:
    PipelineBuilder();

    /// シェーダーステージを設定する
    void SetShaders(VkShaderModule vertModule, VkShaderModule fragModule);

    /// 頂点入力を設定する
    void SetVertexInput(const VkVertexInputBindingDescription& binding,
                        const VkVertexInputAttributeDescription* attributes,
                        uint32_t attributeCount);

    /// 頂点入力なし (スカイボックスなど頂点シェーダーで生成する場合)
    void SetNoVertexInput();

    /// パイプラインレイアウトを設定する
    void SetLayout(VkPipelineLayout layout);

    /// カリングモードを変更する
    void SetCullMode(VkCullModeFlags cullMode);

    /// フロントフェイスを変更する
    void SetFrontFace(VkFrontFace frontFace);

    /// デプステストの設定
    void SetDepthTest(bool enable, bool writeEnable, VkCompareOp compareOp = VK_COMPARE_OP_LESS);

    /// カラーブレンドモードを明示的に設定する
    void SetBlendMode(bool enable, VkBlendFactor srcFactor, VkBlendFactor dstFactor);

    /// 半透明ブレンド: srcAlpha / oneMinusSrcAlpha (ゴースト車両用)
    void SetBlendModeAlpha();

    /// 加算ブレンド: srcAlpha / one (パーティクルの発光表現用)
    void SetBlendModeAdditive();

    /// 深度バイアスを設定する (シャドウマップの shadow acne 防止用)
    void SetDepthBias(bool enable, float constantFactor, float slopeFactor);

    /// パイプラインをビルドする
    VkPipeline Build(VkDevice device, VkRenderPass renderPass, uint32_t subpass = 0);

private:
    VkPipelineShaderStageCreateInfo          shaderStages_[2] = {};
    VkVertexInputBindingDescription          vertexBinding_ = {};
    std::vector<VkVertexInputAttributeDescription> vertexAttributes_;
    bool                                     hasVertexInput_ = false;
    VkPipelineLayout                         layout_ = VK_NULL_HANDLE;
    VkCullModeFlags                          cullMode_ = VK_CULL_MODE_BACK_BIT;
    VkFrontFace                              frontFace_ = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool                                     depthTestEnable_ = true;
    bool                                     depthWriteEnable_ = true;
    VkCompareOp                              depthCompareOp_ = VK_COMPARE_OP_LESS;

    // ブレンド設定
    bool          blendEnable_    = false;
    VkBlendFactor blendSrcFactor_ = VK_BLEND_FACTOR_ONE;
    VkBlendFactor blendDstFactor_ = VK_BLEND_FACTOR_ZERO;

    // 深度バイアス設定
    bool  depthBiasEnable_         = false;
    float depthBiasConstantFactor_ = 0.0f;
    float depthBiasSlopeFactor_    = 0.0f;
};

} // namespace Revora
