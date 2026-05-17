#pragma once

#include "../Math/Matrix4x4.h"
#include "../Math/Vector3.h"
#include "../Resource/MeshLoader.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;

/// ディレクショナルライトのシャドウマップ
/// 深度専用レンダーパスでシーンをライト視点から描画し、
/// メインパスでの影判定に使う深度テクスチャを生成する
///
/// 正射影を使用し、コース全体をカバーする固定範囲を投影する。
/// カスケード分割は不要 (コース半径が限定的なため)。
class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    ShaderManager& shaderMgr);
    void Shutdown();

    /// ライト方向からライト VP 行列を計算する
    /// lightDir はワールド空間のライト方向 (太陽の向き、正規化不要)
    void UpdateLightMatrix(const Vector3& lightDir, const Vector3& sceneCenter);

    /// シャドウパスの開始 (深度専用レンダーパス)
    void BeginPass(VkCommandBuffer cmd);

    /// シャドウパスの終了
    void EndPass(VkCommandBuffer cmd);

    /// シャドウパスでメッシュを描画する (深度のみ書き込み)
    void DrawMesh(VkCommandBuffer cmd,
                  const MeshResource& mesh,
                  const Matrix4x4& modelMatrix);

    const Matrix4x4& GetLightVPMatrix() const { return lightVP_; }
    VkImageView GetDepthImageView() const { return depthImageView_; }
    VkSampler   GetSampler()        const { return sampler_; }

private:
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffer();
    bool CreatePipeline(ShaderManager& shaderMgr);
    bool CreateDescriptorResources();

    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    // 深度テクスチャ
    VkImage        depthImage_     = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_    = VK_NULL_HANDLE;
    VkImageView    depthImageView_ = VK_NULL_HANDLE;
    VkSampler      sampler_        = VK_NULL_HANDLE;

    // レンダーパス・フレームバッファ
    VkRenderPass  renderPass_  = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;

    // パイプライン
    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // ディスクリプタ (ライト VP 行列用 UBO)
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_   = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet_    = VK_NULL_HANDLE;
    VkBuffer              lightUBOBuffer_   = VK_NULL_HANDLE;
    VkDeviceMemory        lightUBOMemory_   = VK_NULL_HANDLE;
    void*                 lightUBOMapped_   = nullptr;

    // ライト行列
    Matrix4x4 lightVP_ = Matrix4x4::Identity();

    // シャドウマップ解像度とカバー範囲
    static constexpr uint32_t kShadowMapSize   = 2048;
    static constexpr float    kOrthoHalfExtent  = 50.0f;
    static constexpr float    kLightDistance     = 80.0f;
};

} // namespace Revora
