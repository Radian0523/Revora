#pragma once

#include "../Math/Matrix4x4.h"
#include "../Resource/TextureLoader.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Revora {

class ShaderManager;

/// 2D スプライトのバッチ描画レンダラー
/// 全 UI 要素の頂点を 1 つの頂点バッファにまとめて 1 回の vkCmdDraw で描画する。
/// ParticleRenderer と同じダブルバッファリングパターンを採用しつつ、
/// テクスチャサンプラーバインディングを追加して文字・アイコンの描画に対応する
class SpriteRenderer {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    bool Initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkRenderPass renderPass,
                    ShaderManager& shaderMgr,
                    uint32_t maxVertices);
    void Shutdown();

    /// 正射影行列を画面サイズに合わせて更新する
    void UpdateProjection(uint32_t frameIndex,
                          float screenWidth, float screenHeight);

    /// CPU で生成した頂点データを GPU バッファに転送する
    void UploadVertices(uint32_t frameIndex,
                        const void* vertices, uint32_t vertexCount);

    /// フォントアトラステクスチャをバインドして全頂点を描画する
    void Draw(VkCommandBuffer cmd, uint32_t frameIndex,
              const TextureResource& texture);

private:
    /// GPU 頂点フォーマット (32 bytes/vertex)
    struct SpriteVertex {
        float position[2];  // スクリーン座標 (ピクセル)
        float texCoord[2];  // UV
        float color[4];     // RGBA
    };

    bool CreateDescriptorResources();

    static constexpr uint32_t kMaxFramesInFlight = 2;

    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VkPipeline       pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // ディスクリプタ (正射影 UBO + テクスチャサンプラー)
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_   = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSets_[kMaxFramesInFlight] = {};

    // UBO (正射影行列)
    VkBuffer       projUBOBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory projUBOMemories_[kMaxFramesInFlight] = {};
    void*          projUBOMapped_[kMaxFramesInFlight] = {};

    // 頂点バッファ (フレームごとにダブルバッファリング)
    VkBuffer       vertexBuffers_[kMaxFramesInFlight] = {};
    VkDeviceMemory vertexMemories_[kMaxFramesInFlight] = {};
    void*          vertexMapped_[kMaxFramesInFlight] = {};

    uint32_t vertexCount_[kMaxFramesInFlight] = {};
    uint32_t maxVertices_ = 0;

    // テクスチャ変更検出: フレームごとに前回バインドした VkImageView を記録し、
    // 変更時のみディスクリプタセットを更新する
    VkImageView lastBoundTextureView_[kMaxFramesInFlight] = {};
};

} // namespace Revora
