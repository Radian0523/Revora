#include "Vertex.h"

#include <cstddef>

namespace Revora {

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
    VkVertexInputBindingDescription binding = {};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attrs = {};

    // location 0: position (vec3)
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, position);

    // location 1: normal (vec3)
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, normal);

    // location 2: texCoord (vec2)
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset   = offsetof(Vertex, texCoord);

    return attrs;
}

} // namespace Revora
