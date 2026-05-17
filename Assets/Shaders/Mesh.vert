#version 450

// UBO: フレーム共通のシーンデータ
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDirection;   // xyz: 方向, w: 未使用
    mat4 lightVP;          // シャドウマップ用ライト VP 行列
    vec4 cameraPosition;   // xyz: カメラ位置, w: 未使用
} scene;

// プッシュ定数: オブジェクト固有データ
layout(push_constant) uniform PushConstants {
    mat4  model;
    float alpha;          // 半透明度 (1.0 = 不透明, 0.4 = ゴースト)
    float reflectivity;   // 環境マップ反射率 (0.0 = 反射なし)
    float padding0;
    float padding1;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec4 fragLightSpacePos;

void main() {
    // C++ 行優先メモリ -> GLSL 列優先解釈で暗黙転置される
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = scene.projection * scene.view * worldPos;

    // ワールド空間の法線 (非均一スケール未対応)
    fragNormal   = mat3(pc.model) * inNormal;
    fragTexCoord = inTexCoord;

    // 環境マップ反射とシャドウ座標に必要なワールド座標
    fragWorldPos = worldPos.xyz;

    // ライト空間座標 (シャドウ判定用)
    fragLightSpacePos = scene.lightVP * worldPos;
}
