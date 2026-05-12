#version 450

// UBO: フレーム共通のシーンデータ
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDirection;  // xyz: 方向, w: 未使用
} scene;

// プッシュ定数: オブジェクト固有の Model 行列
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    // C++ 行優先メモリ → GLSL 列優先解釈で暗黙転置される
    // GLSL 側の projection * view * model は
    // C++ 側の model * view * projection (行ベクトル方式) と等価
    gl_Position = scene.projection * scene.view * pc.model * vec4(inPosition, 1.0);

    // ワールド空間の法線 (非均一スケール未対応、Phase 2 では等倍のみ)
    fragNormal   = mat3(pc.model) * inNormal;
    fragTexCoord = inTexCoord;
}
