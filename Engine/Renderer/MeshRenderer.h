#pragma once

#include "../Math/Matrix4x4.h"
#include "../Resource/MeshLoader.h"
#include "../Resource/TextureLoader.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;

/// フレーム共通のシーンデータ (UBO binding 0)
/// View, Projection に加え、シャドウマップと環境マップ反射に必要な情報を持つ
struct SceneUBO {
    float view[16];
    float projection[16];
    float lightDirection[4];  // vec3 + padding
    float lightVP[16];        // シャドウマップ用ライト VP 行列
    float cameraPosition[4];  // vec3 + padding (環境マップ反射用)
};

/// メッシュ描画時のプッシュ定数
/// モデル行列に加え、半透明と環境マップ反射のパラメータを持つ
struct MeshPushConstants {
    Matrix4x4 model;         // 64 bytes
    float     alpha;         // 4 bytes (1.0=不透明, 0.4=ゴースト)
    float     reflectivity;  // 4 bytes (0.0=反射なし, 0.3=車両)
    float     padding[2];    // 8 bytes (16バイトアライメント)
};
// 合計 80 bytes: プッシュ定数の上限 128 bytes 以内

/// テクスチャ付きメッシュの描画を担当するレンダラー
/// 独自のディスクリプタレイアウト (4 バインディング) を管理し、
/// シャドウマップ・環境マップ・半透明描画をサポートする
///
/// Skybox は既存の DescriptorSetManager (2 バインディング) を引き続き使用するため、
/// MeshRenderer のディスクリプタ変更が Skybox に影響を与えない設計になっている
class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkRenderPass renderPass,
                    ShaderManager& shaderMgr);
    void Shutdown();

    /// シャドウマップの深度テクスチャを設定する
    void SetShadowMap(VkImageView shadowView, VkSampler shadowSampler);

    /// 環境マップ (キューブマップ) を設定する
    void SetEnvironmentMap(VkImageView envView, VkSampler envSampler);

    /// フレーム開始時にシーンデータを更新する
    void UpdateSceneData(uint32_t frameIndex,
                         const Matrix4x4& view,
                         const Matrix4x4& projection,
                         const float lightDir[3],
                         const Matrix4x4& lightVP,
                         const float cameraPos[3]);

    /// 不透明メッシュを描画する
    void Draw(VkCommandBuffer cmd,
              uint32_t frameIndex,
              const MeshResource& mesh,
              const TextureResource& texture,
              const Matrix4x4& modelMatrix,
              float reflectivity = 0.0f);

    /// 半透明メッシュを描画する (アルファブレンド)
    void DrawTransparent(VkCommandBuffer cmd,
                         uint32_t frameIndex,
                         const MeshResource& mesh,
                         const TextureResource& texture,
                         const Matrix4x4& modelMatrix,
                         float alpha);

private:
    bool CreateDescriptorResources();
    bool CreateDummyTextures();
    void UpdateAllDescriptorSets(uint32_t frameIndex,
                                  const TextureResource& texture);

    static constexpr uint32_t kMaxFramesInFlight = 2;

    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    // パイプライン
    VkPipeline       opaquePipeline_      = VK_NULL_HANDLE;
    VkPipeline       transparentPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_      = VK_NULL_HANDLE;

    // 独自ディスクリプタ管理
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_   = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSets_[kMaxFramesInFlight] = {};

    // UBO (フレームごとにダブルバッファリング)
    VkBuffer       sceneUBOBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory sceneUBOMemories_[kMaxFramesInFlight] = {};
    void*          sceneUBOMapped_[kMaxFramesInFlight] = {};

    // テクスチャ差し替え追跡 (変更時のみディスクリプタを更新)
    VkImageView lastBoundTextureView_[kMaxFramesInFlight] = {};

    // シャドウマップ / 環境マップ (外部から設定される)
    VkImageView shadowMapView_    = VK_NULL_HANDLE;
    VkSampler   shadowMapSampler_ = VK_NULL_HANDLE;
    VkImageView envMapView_       = VK_NULL_HANDLE;
    VkSampler   envMapSampler_    = VK_NULL_HANDLE;

    // ダミーテクスチャ: シャドウマップや環境マップが未設定時に使用
    // Vulkan はバインドされていないディスクリプタへのアクセスが未定義動作のため必須
    VkImage        dummyImage2D_      = VK_NULL_HANDLE;
    VkDeviceMemory dummyMemory2D_     = VK_NULL_HANDLE;
    VkImageView    dummyView2D_       = VK_NULL_HANDLE;
    VkImage        dummyCubeImage_    = VK_NULL_HANDLE;
    VkDeviceMemory dummyCubeMemory_   = VK_NULL_HANDLE;
    VkImageView    dummyCubeView_     = VK_NULL_HANDLE;
    VkSampler      dummySampler_      = VK_NULL_HANDLE;
};

} // namespace Revora
