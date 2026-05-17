#version 450

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDirection;
    mat4 lightVP;
    vec4 cameraPosition;
} scene;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;
layout(set = 0, binding = 3) uniform samplerCube envMap;

layout(push_constant) uniform PushConstants {
    mat4  model;
    float alpha;
    float reflectivity;
    float padding0;
    float padding1;
} pc;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragLightSpacePos;

layout(location = 0) out vec4 outColor;

/// 3x3 PCF フィルタによるソフトシャドウ
/// 周囲 9 テクセルの深度を比較し、平均の遮蔽率を返す
float CalculateShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    // パースペクティブ除算 (正射影なので w=1 のはずだが安全のため)
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // NDC [-1,1] -> テクスチャ座標 [0,1]
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5;

    // シャドウマップ範囲外は影なし
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;

    // 法線とライト方向の角度に応じたバイアスで shadow acne を軽減
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    // 3x3 PCF カーネル
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

/// Schlick の Fresnel 近似
/// 浅い角度ほど反射が強くなる効果を再現する
float FresnelSchlick(float cosTheta)
{
    // F0 = 0.04 (非金属の典型的な基底反射率)
    float f0 = 0.04;
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    vec3 normal   = normalize(fragNormal);
    vec3 lightDir = normalize(scene.lightDirection.xyz);

    // Lambert ディフューズ
    float diffuse = max(dot(normal, lightDir), 0.0);

    // シャドウ
    float shadow = CalculateShadow(fragLightSpacePos, normal, lightDir);

    // ライティング: アンビエント + ディフューズ * シャドウ
    float ambient  = 0.15;
    float lighting = ambient + diffuse * 0.85 * shadow;

    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 litColor = texColor.rgb * lighting;

    // 環境マップ反射 (reflectivity > 0 のオブジェクトのみ)
    if (pc.reflectivity > 0.0) {
        vec3 viewDir    = normalize(fragWorldPos - scene.cameraPosition.xyz);
        vec3 reflectDir = reflect(viewDir, normal);
        vec3 envColor   = texture(envMap, reflectDir).rgb;

        // Fresnel 効果: 浅い角度ほど反射が強い
        float cosTheta = max(dot(-viewDir, normal), 0.0);
        float fresnel  = FresnelSchlick(cosTheta);

        litColor = mix(litColor, envColor, fresnel * pc.reflectivity);
    }

    outColor = vec4(litColor, pc.alpha);
}
