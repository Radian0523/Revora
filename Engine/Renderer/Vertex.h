#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Revora {

/// メッシュ用頂点フォーマット
/// position (vec3) + normal (vec3) + texCoord (vec2) の 32 bytes
struct Vertex {
    float position[3];
    float normal[3];
    float texCoord[2];

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};

} // namespace Revora
