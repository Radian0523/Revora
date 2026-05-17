#version 450

// UBO: 正射影行列 (ピクセル座標 → NDC)
layout(set = 0, binding = 0) uniform ProjectionUBO {
    mat4 projection;
} ubo;

// 頂点入力: スクリーン座標のクアッド
layout(location = 0) in vec2 inPosition;   // ピクセル座標
layout(location = 1) in vec2 inTexCoord;   // UV
layout(location = 2) in vec4 inColor;      // RGBA

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    gl_Position  = ubo.projection * vec4(inPosition, 0.0, 1.0);
    fragTexCoord = inTexCoord;
    fragColor    = inColor;
}
