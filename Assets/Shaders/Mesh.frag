#version 450

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 projection;
    vec4 lightDirection;
} scene;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Lambert ライティング
    vec3 normal   = normalize(fragNormal);
    vec3 lightDir = normalize(scene.lightDirection.xyz);
    float diffuse = max(dot(normal, lightDir), 0.0);

    // アンビエント + ディフューズ
    float ambient = 0.15;
    float lighting = ambient + diffuse * 0.85;

    vec4 texColor = texture(texSampler, fragTexCoord);
    outColor = vec4(texColor.rgb * lighting, texColor.a);
}
