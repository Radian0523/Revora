#include "ShaderManager.h"

#include <SDL.h>
#include <fstream>

namespace Revora {

ShaderManager::~ShaderManager() {
    Shutdown();
}

void ShaderManager::Initialize(VkDevice device) {
    device_ = device;
}

void ShaderManager::Shutdown() {
    for (auto& [path, module] : cache_) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module, nullptr);
        }
    }
    cache_.clear();
    device_ = VK_NULL_HANDLE;
}

VkShaderModule ShaderManager::GetShaderModule(const std::string& spvPath) {
    // キャッシュを確認
    auto it = cache_.find(spvPath);
    if (it != cache_.end()) {
        return it->second;
    }

    // ファイル読み込み
    auto code = ReadSPIRVFile(spvPath);
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }

    // シェーダーモジュール生成
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    cache_[spvPath] = module;
    return module;
}

std::vector<char> ShaderManager::ReadSPIRVFile(const std::string& filepath) {
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open shader: %s", fullPath.c_str());
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

} // namespace Revora
