#version 450

// ライト視点の VP 行列 (正射影)
layout(set = 0, binding = 0) uniform LightUBO {
    mat4 lightVP;
} light;

// オブジェクト固有の Model 行列
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;    // シャドウパスでは未使用だがバインディング互換のため
layout(location = 2) in vec2 inTexCoord;  // 同上

void main() {
    gl_Position = light.lightVP * pc.model * vec4(inPosition, 1.0);
}
