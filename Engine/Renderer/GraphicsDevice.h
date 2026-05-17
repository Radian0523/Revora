#pragma once

#include "../Math/Matrix4x4.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Revora {

class Window;

class GraphicsDevice {
public:
    GraphicsDevice() = default;
    ~GraphicsDevice();

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    bool Initialize(const Window& window);
    void Shutdown();

    /// フレーム開始: フェンス同期 + イメージ取得 + コマンドバッファ記録開始
    /// BeginMainPass() を呼ぶまでの間にシャドウパスなど別レンダーパスを実行できる
    void BeginFrame();

    /// メインレンダーパス開始: クリアカラーを指定してメインパスを開始
    void BeginMainPass(float r, float g, float b, float a);

    /// フレーム終了: メインパス終了 + コマンド送信 + プレゼンテーション
    void EndFrame();

    /// テスト三角形の描画 (Phase 1 用、Phase 2 完了後に除去)
    void DrawTestTriangle(const Matrix4x4& mvp);

    // --- 他のシステムが必要とする Vulkan ハンドルのアクセサ ---
    VkDevice         GetDevice()         const { return device_; }
    VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice_; }
    VkQueue          GetGraphicsQueue()  const { return graphicsQueue_; }
    VkCommandPool    GetCommandPool()    const { return commandPool_; }
    VkRenderPass     GetRenderPass()     const { return renderPass_; }
    VkExtent2D       GetSwapChainExtent() const { return swapChainExtent_; }
    uint32_t         GetCurrentFrame()   const { return currentFrame_; }
    VkCommandBuffer  GetCurrentCommandBuffer() const { return commandBuffers_[currentFrame_]; }

    static constexpr uint32_t GetMaxFramesInFlight() { return kMaxFramesInFlight; }

private:
    // --- 初期化/解放メソッド (生成順に定義) ---
    bool CreateInstance(const Window& window);
    bool CreateSurface(const Window& window);
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapChain(int width, int height);
    bool CreateRenderPass();
    bool CreateDepthResources(int width, int height);
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    bool CreateTestTrianglePipeline();
    bool CreateTestTriangleVertexBuffer();

    void DestroyTestTriangleVertexBuffer();
    void DestroyTestTrianglePipeline();
    void DestroySyncObjects();
    void DestroyCommandBuffers();
    void DestroyCommandPool();
    void DestroyFramebuffers();
    void DestroyDepthResources();
    void DestroyRenderPass();
    void DestroySwapChain();
    void DestroyLogicalDevice();
    void DestroySurface();
    void DestroyInstance();

    // --- ユーティリティ ---
    VkFormat FindDepthFormat() const;
    std::vector<char> ReadSPIRVFile(const char* filepath) const;

    // --- Vulkan コアオブジェクト ---
    VkInstance               instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR             surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice_ = VK_NULL_HANDLE;
    VkDevice                 device_         = VK_NULL_HANDLE;
    VkQueue                  graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue                  presentQueue_   = VK_NULL_HANDLE;
    uint32_t                 graphicsFamily_ = 0;
    uint32_t                 presentFamily_  = 0;

    // スワップチェーン
    VkSwapchainKHR           swapChain_      = VK_NULL_HANDLE;
    VkFormat                 swapChainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapChainExtent_ = {};
    std::vector<VkImage>     swapChainImages_;
    std::vector<VkImageView> swapChainImageViews_;

    // デプスバッファ
    VkImage                  depthImage_     = VK_NULL_HANDLE;
    VkDeviceMemory           depthMemory_    = VK_NULL_HANDLE;
    VkImageView              depthImageView_ = VK_NULL_HANDLE;
    VkFormat                 depthFormat_    = VK_FORMAT_UNDEFINED;

    // レンダーパス・フレームバッファ
    VkRenderPass             renderPass_     = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // コマンドバッファ
    VkCommandPool            commandPool_    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // フレーム同期 (ダブルバッファリング)
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::vector<VkSemaphore>  imageAvailableSemaphores_;
    std::vector<VkSemaphore>  renderFinishedSemaphores_;
    std::vector<VkFence>      inFlightFences_;
    uint32_t                  currentFrame_  = 0;
    uint32_t                  imageIndex_    = 0;

    // テスト三角形リソース
    VkPipeline               pipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout         pipelineLayout_ = VK_NULL_HANDLE;
    VkBuffer                 vertexBuffer_   = VK_NULL_HANDLE;
    VkDeviceMemory           vertexMemory_   = VK_NULL_HANDLE;

#ifdef REVORA_ENABLE_VALIDATION
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    bool CreateDebugMessenger();
    void DestroyDebugMessenger();
#endif
};

} // namespace Revora
