#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace Revora {

/// SPIR-V シェーダーファイルの読み込みとキャッシュ
/// 同じファイルパスに対して VkShaderModule を再利用する
class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    void Initialize(VkDevice device);
    void Shutdown();

    /// SPIR-V ファイルからシェーダーモジュールを取得する (キャッシュ付き)
    /// 実行ファイルのベースパスからの相対パスで指定
    VkShaderModule GetShaderModule(const std::string& spvPath);

private:
    static std::vector<char> ReadSPIRVFile(const std::string& filepath);

    VkDevice device_ = VK_NULL_HANDLE;
    std::unordered_map<std::string, VkShaderModule> cache_;
};

} // namespace Revora
