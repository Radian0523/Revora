#include "GraphicsDevice.h"
#include "../Platform/Window.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace Revora {

// テスト三角形の頂点データ
struct TriangleVertex {
    float position[3];
    float color[4];
};

// --- 公開 API ---

GraphicsDevice::~GraphicsDevice() {
    Shutdown();
}

bool GraphicsDevice::Initialize(const Window& window) {
    if (!CreateInstance(window))                                     { return false; }
#ifdef REVORA_ENABLE_VALIDATION
    CreateDebugMessenger();
#endif
    if (!CreateSurface(window))                               { Shutdown(); return false; }
    if (!SelectPhysicalDevice())                              { Shutdown(); return false; }
    if (!CreateLogicalDevice())                               { Shutdown(); return false; }
    if (!CreateSwapChain(window.GetWidth(), window.GetHeight())) { Shutdown(); return false; }
    if (!CreateRenderPass())                                  { Shutdown(); return false; }
    if (!CreateDepthResources(window.GetWidth(), window.GetHeight())) { Shutdown(); return false; }
    if (!CreateFramebuffers())                                { Shutdown(); return false; }
    if (!CreateCommandPool())                                 { Shutdown(); return false; }
    if (!CreateCommandBuffers())                              { Shutdown(); return false; }
    if (!CreateSyncObjects())                                 { Shutdown(); return false; }
    if (!CreateTestTrianglePipeline())                        { Shutdown(); return false; }
    if (!CreateTestTriangleVertexBuffer())                    { Shutdown(); return false; }

    return true;
}

void GraphicsDevice::Shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    // 生成の逆順で解放
    DestroyTestTriangleVertexBuffer();
    DestroyTestTrianglePipeline();
    DestroySyncObjects();
    DestroyCommandBuffers();
    DestroyCommandPool();
    DestroyFramebuffers();
    DestroyDepthResources();
    DestroyRenderPass();
    DestroySwapChain();
    DestroyLogicalDevice();
    DestroySurface();
#ifdef REVORA_ENABLE_VALIDATION
    DestroyDebugMessenger();
#endif
    DestroyInstance();
}

void GraphicsDevice::BeginFrame() {
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
                          imageAvailableSemaphores_[currentFrame_],
                          VK_NULL_HANDLE, &imageIndex_);

    // フェンスのリセットはイメージ取得成功後に行う。
    // 取得前にリセットすると、取得失敗時にフェンスがシグナルされず次フレームでデッドロックする。
    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
}

void GraphicsDevice::BeginMainPass(float r, float g, float b, float a) {
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];

    // レンダーパス開始
    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{r, g, b, a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = renderPass_;
    rpInfo.framebuffer       = framebuffers_[imageIndex_];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapChainExtent_;
    rpInfo.clearValueCount   = 2;
    rpInfo.pClearValues      = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ビューポート設定 (Y 反転で DX 規約と統一)
    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = static_cast<float>(swapChainExtent_.height);
    viewport.width    = static_cast<float>(swapChainExtent_.width);
    viewport.height   = -static_cast<float>(swapChainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent_;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void GraphicsDevice::EndFrame() {
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // コマンドバッファの送信
    VkSemaphore waitSemaphores[]   = {imageAvailableSemaphores_[currentFrame_]};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]);

    // プレゼンテーション
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapChain_;
    presentInfo.pImageIndices      = &imageIndex_;

    vkQueuePresentKHR(presentQueue_, &presentInfo);

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void GraphicsDevice::DrawTestTriangle(const Matrix4x4& mvp) {
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    // C++ 行優先メモリ → GLSL 列優先解釈で暗黙的に転置される。
    // GLSL 側で M * v (列ベクトル方式) を計算すると、
    // C++ 側の v * M (行ベクトル方式) と同じ結果になる。明示的な転置は不要。
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(Matrix4x4), &mvp);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer_, &offset);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

// --- Instance 生成 ---

bool GraphicsDevice::CreateInstance(const Window& window) {
    VkApplicationInfo appInfo = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Revora";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Revora Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    std::vector<const char*> extensions;

    // macOS MoltenVK 対応: portability enumeration 拡張を追加
    // MoltenVK 以外の環境では不要なので、利用可能な場合のみ有効化
    VkInstanceCreateFlags instanceFlags = 0;

    uint32_t availableExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(availableExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, availableExts.data());

    for (const auto& ext : availableExts) {
        if (std::strcmp(ext.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            instanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        // portability_subset デバイス拡張が依存するインスタンス拡張
        if (std::strcmp(ext.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
    }

#ifdef REVORA_ENABLE_VALIDATION
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // SDL が要求するサーフェス拡張を取得 (プラットフォーム非依存)
    SDL_Window* sdlWindow = window.GetSDLWindow();
    unsigned int sdlExtCount = 0;
    SDL_Vulkan_GetInstanceExtensions(sdlWindow, &sdlExtCount, nullptr);
    size_t baseCount = extensions.size();
    extensions.resize(baseCount + sdlExtCount);
    SDL_Vulkan_GetInstanceExtensions(sdlWindow, &sdlExtCount, extensions.data() + baseCount);

    std::vector<const char*> layers;
#ifdef REVORA_ENABLE_VALIDATION
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames     = layers.data();
    createInfo.flags                   = instanceFlags;

    return vkCreateInstance(&createInfo, nullptr, &instance_) == VK_SUCCESS;
}

// --- サーフェス生成 ---

bool GraphicsDevice::CreateSurface(const Window& window) {
    // SDL_Vulkan_CreateSurface で macOS/Windows/Linux 共通処理
    return SDL_Vulkan_CreateSurface(window.GetSDLWindow(), instance_, &surface_) == SDL_TRUE;
}

// --- 物理デバイス選択 ---

bool GraphicsDevice::SelectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto& candidate : devices) {
        // グラフィックス・プレゼンテーション対応のキューファミリーを検索
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, families.data());

        bool foundGraphics = false;
        bool foundPresent  = false;

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamily_ = i;
                foundGraphics = true;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &presentSupport);
            if (presentSupport) {
                presentFamily_ = i;
                foundPresent = true;
            }

            if (foundGraphics && foundPresent) {
                break;
            }
        }

        if (foundGraphics && foundPresent) {
            physicalDevice_ = candidate;
            return true;
        }
    }

    return false;
}

// --- 論理デバイス生成 ---

bool GraphicsDevice::CreateLogicalDevice() {
    // キューの作成情報 (グラフィックスとプレゼンテーションが同一ファミリーの場合も考慮)
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;

    // 重複するファミリーインデックスを排除
    std::vector<uint32_t> uniqueFamilies = {graphicsFamily_};
    if (presentFamily_ != graphicsFamily_) {
        uniqueFamilies.push_back(presentFamily_);
    }

    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    // デバイス拡張
    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // MoltenVK 対応: portability_subset とその依存拡張を有効化
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount, availableExtensions.data());

    bool hasPortabilitySubset = false;
    bool hasGetPhysDevProps2  = false;
    for (const auto& ext : availableExtensions) {
        if (std::strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0) {
            hasPortabilitySubset = true;
        }
        if (std::strcmp(ext.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
            hasGetPhysDevProps2 = true;
        }
    }

    if (hasPortabilitySubset) {
        deviceExtensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentFamily_,  0, &presentQueue_);
    return true;
}

// --- スワップチェーン生成 ---

bool GraphicsDevice::CreateSwapChain(int width, int height) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    // サーフェスフォーマット選択: B8G8R8A8_SRGB を優先
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = fmt;
            break;
        }
    }

    // プレゼントモード選択: MAILBOX を優先、なければ FIFO (V-Sync)
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());

    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenPresentMode = mode;
            break;
        }
    }

    // スワップチェーンのエクステント
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width  = std::clamp(static_cast<uint32_t>(width),
                                   capabilities.minImageExtent.width,
                                   capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height),
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    // ダブルバッファリング: 最小イメージ数 + 1 (上限考慮)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface_;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = chosenFormat.format;
    createInfo.imageColorSpace  = chosenFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform     = capabilities.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = chosenPresentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = VK_NULL_HANDLE;

    // グラフィックスとプレゼンテーションのキューファミリーが異なる場合
    uint32_t familyIndices[] = {graphicsFamily_, presentFamily_};
    if (graphicsFamily_ != presentFamily_) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = familyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
        return false;
    }

    swapChainFormat_ = chosenFormat.format;
    swapChainExtent_ = extent;

    // スワップチェーンイメージの取得
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

    // イメージビューの生成
    swapChainImageViews_.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = swapChainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = swapChainFormat_;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device_, &viewInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

// --- レンダーパス生成 ---

bool GraphicsDevice::CreateRenderPass() {
    depthFormat_ = FindDepthFormat();

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = swapChainFormat_;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format         = depthFormat_;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // サブパス依存: プレゼンテーション → 描画
    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments    = attachments;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    return vkCreateRenderPass(device_, &rpInfo, nullptr, &renderPass_) == VK_SUCCESS;
}

// --- デプスリソース生成 ---

bool GraphicsDevice::CreateDepthResources(int width, int height) {
    // デプスイメージ生成
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = depthFormat_;
    imageInfo.extent.width  = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device_, &imageInfo, nullptr, &depthImage_) != VK_SUCCESS) {
        return false;
    }

    // メモリ割り当て
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, depthImage_, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &depthMemory_) != VK_SUCCESS) {
        return false;
    }

    vkBindImageMemory(device_, depthImage_, depthMemory_, 0);

    // デプスイメージビュー生成
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = depthFormat_;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    return vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_) == VK_SUCCESS;
}

// --- フレームバッファ生成 ---

bool GraphicsDevice::CreateFramebuffers() {
    framebuffers_.resize(swapChainImageViews_.size());

    for (size_t i = 0; i < swapChainImageViews_.size(); ++i) {
        VkImageView attachments[] = {swapChainImageViews_[i], depthImageView_};

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = renderPass_;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = swapChainExtent_.width;
        fbInfo.height          = swapChainExtent_.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(device_, &fbInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

// --- コマンドプール生成 ---

bool GraphicsDevice::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;

    return vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) == VK_SUCCESS;
}

// --- コマンドバッファ生成 ---

bool GraphicsDevice::CreateCommandBuffers() {
    commandBuffers_.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;

    return vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) == VK_SUCCESS;
}

// --- 同期オブジェクト生成 ---

bool GraphicsDevice::CreateSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    renderFinishedSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // 初回フレームでデッドロックしないよう、シグナル済み状態で生成
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &semInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

// --- テスト三角形パイプライン生成 ---

bool GraphicsDevice::CreateTestTrianglePipeline() {
    // SPIR-V シェーダーの読み込み
    auto vertCode = ReadSPIRVFile("Assets/Shaders/Triangle.vert.spv");
    auto fragCode = ReadSPIRVFile("Assets/Shaders/Triangle.frag.spv");
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    // シェーダーモジュール生成
    VkShaderModuleCreateInfo vertModuleInfo = {};
    vertModuleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.codeSize = vertCode.size();
    vertModuleInfo.pCode    = reinterpret_cast<const uint32_t*>(vertCode.data());

    VkShaderModule vertModule;
    if (vkCreateShaderModule(device_, &vertModuleInfo, nullptr, &vertModule) != VK_SUCCESS) {
        return false;
    }

    VkShaderModuleCreateInfo fragModuleInfo = {};
    fragModuleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.codeSize = fragCode.size();
    fragModuleInfo.pCode    = reinterpret_cast<const uint32_t*>(fragCode.data());

    VkShaderModule fragModule;
    if (vkCreateShaderModule(device_, &fragModuleInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, vertModule, nullptr);
        return false;
    }

    // シェーダーステージ
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName  = "main";

    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName  = "main";

    // 頂点入力: position (float3) + color (float4)
    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(TriangleVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2] = {};
    attrDescs[0].binding  = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset   = offsetof(TriangleVertex, position);

    attrDescs[1].binding  = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrDescs[1].offset   = offsetof(TriangleVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount    = 1;
    vertexInputInfo.pVertexBindingDescriptions       = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount  = 2;
    vertexInputInfo.pVertexAttributeDescriptions     = attrDescs;

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
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // マルチサンプリング (無効)
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // デプス・ステンシル
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = VK_TRUE;
    depthStencil.depthWriteEnable      = VK_TRUE;
    depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
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

    // パイプラインレイアウト (プッシュ定数: MVP 行列 64 bytes)
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(Matrix4x4);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushConstantRange;

    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragModule, nullptr);
        vkDestroyShaderModule(device_, vertModule, nullptr);
        return false;
    }

    // グラフィックスパイプライン生成
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = pipelineLayout_;
    pipelineInfo.renderPass          = renderPass_;
    pipelineInfo.subpass             = 0;

    VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo,
                                                nullptr, &pipeline_);

    // シェーダーモジュールはパイプライン生成後に不要
    vkDestroyShaderModule(device_, fragModule, nullptr);
    vkDestroyShaderModule(device_, vertModule, nullptr);

    return result == VK_SUCCESS;
}

// --- テスト三角形頂点バッファ生成 ---

bool GraphicsDevice::CreateTestTriangleVertexBuffer() {
    // 頂点データ: RGB三角形
    TriangleVertex vertices[] = {
        {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // 上: 赤
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},  // 右下: 緑
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // 左下: 青
    };

    VkDeviceSize bufferSize = sizeof(vertices);

    // ホスト可視バッファを直接使用 (テスト三角形はステージングバッファ不要)
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = bufferSize;
    bufferInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &vertexBuffer_) != VK_SUCCESS) {
        return false;
    }

    // メモリ割り当て
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, vertexBuffer_, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    // ホスト可視 + コヒーレントなメモリタイプを検索
    uint32_t memTypeIndex = UINT32_MAX;
    VkMemoryPropertyFlags requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &vertexMemory_) != VK_SUCCESS) {
        return false;
    }

    vkBindBufferMemory(device_, vertexBuffer_, vertexMemory_, 0);

    // 頂点データのコピー
    void* data = nullptr;
    vkMapMemory(device_, vertexMemory_, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices, bufferSize);
    vkUnmapMemory(device_, vertexMemory_);

    return true;
}

// --- 解放メソッド群 ---

void GraphicsDevice::DestroyTestTriangleVertexBuffer() {
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vertexBuffer_ = VK_NULL_HANDLE;
    }
    if (vertexMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, vertexMemory_, nullptr);
        vertexMemory_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroyTestTrianglePipeline() {
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroySyncObjects() {
    for (auto& sem : imageAvailableSemaphores_) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, sem, nullptr);
        }
    }
    imageAvailableSemaphores_.clear();

    for (auto& sem : renderFinishedSemaphores_) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, sem, nullptr);
        }
    }
    renderFinishedSemaphores_.clear();

    for (auto& fence : inFlightFences_) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device_, fence, nullptr);
        }
    }
    inFlightFences_.clear();
}

void GraphicsDevice::DestroyCommandBuffers() {
    if (!commandBuffers_.empty() && commandPool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, commandPool_,
                             static_cast<uint32_t>(commandBuffers_.size()),
                             commandBuffers_.data());
        commandBuffers_.clear();
    }
}

void GraphicsDevice::DestroyCommandPool() {
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroyFramebuffers() {
    for (auto& fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, fb, nullptr);
        }
    }
    framebuffers_.clear();
}

void GraphicsDevice::DestroyDepthResources() {
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, depthMemory_, nullptr);
        depthMemory_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroyRenderPass() {
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroySwapChain() {
    for (auto& view : swapChainImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view, nullptr);
        }
    }
    swapChainImageViews_.clear();
    swapChainImages_.clear();

    if (swapChain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapChain_, nullptr);
        swapChain_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroyLogicalDevice() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_  = VK_NULL_HANDLE;
}

void GraphicsDevice::DestroySurface() {
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

void GraphicsDevice::DestroyInstance() {
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

// --- ユーティリティ ---

VkFormat GraphicsDevice::FindDepthFormat() const {
    // D32_SFLOAT を優先 (macOS/MoltenVK で確実にサポート)
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    // フォールバック (到達しないはず)
    return VK_FORMAT_D32_SFLOAT;
}

std::vector<char> GraphicsDevice::ReadSPIRVFile(const char* filepath) const {
    // 実行ファイルのディレクトリを基準にパスを構築
    std::string fullPath;
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        fullPath = basePath;
        SDL_free(basePath);
    }
    fullPath += filepath;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

// --- デバッグメッセンジャー (バリデーションレイヤー有効時のみ) ---

#ifdef REVORA_ENABLE_VALIDATION

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*userData*/)
{
    // WARNING 以上のメッセージのみ stderr に出力
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Vulkan Validation: %s", callbackData->pMessage);
    }
    return VK_FALSE;
}

bool GraphicsDevice::CreateDebugMessenger() {
    auto createFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!createFunc) {
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    return createFunc(instance_, &createInfo, nullptr, &debugMessenger_) == VK_SUCCESS;
}

void GraphicsDevice::DestroyDebugMessenger() {
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFunc) {
            destroyFunc(instance_, debugMessenger_, nullptr);
        }
        debugMessenger_ = VK_NULL_HANDLE;
    }
}

#endif // REVORA_ENABLE_VALIDATION

} // namespace Revora
