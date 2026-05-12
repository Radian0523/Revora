#version 450

layout(set = 0, binding = 0) uniform SkyboxUBO {
    mat4 view;
    mat4 projection;
} scene;

layout(location = 0) out vec3 fragTexCoord;

// 頂点シェーダー内でキューブ頂点を生成する (頂点バッファ不要)
// gl_VertexIndex 0-35 で 12 三角形 = 6面のキューブを構成
void main() {
    // キューブの 8 頂点
    vec3 positions[8] = vec3[](
        vec3(-1.0, -1.0, -1.0),
        vec3( 1.0, -1.0, -1.0),
        vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0, -1.0),
        vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0,  1.0),
        vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0,  1.0)
    );

    // 36 頂点のインデックス (12 三角形)
    int indices[36] = int[](
        // +Z face
        4, 5, 6,  6, 7, 4,
        // -Z face
        1, 0, 3,  3, 2, 1,
        // +X face
        5, 1, 2,  2, 6, 5,
        // -X face
        0, 4, 7,  7, 3, 0,
        // +Y face
        3, 7, 6,  6, 2, 3,
        // -Y face
        0, 1, 5,  5, 4, 0
    );

    vec3 pos = positions[indices[gl_VertexIndex]];

    // サンプリング方向 = 頂点位置そのもの (キューブの中心が原点)
    fragTexCoord = pos;

    // C++ 行優先メモリ → GLSL 列優先解釈で暗黙転置
    vec4 clipPos = scene.projection * scene.view * vec4(pos, 1.0);

    // デプス値を 1.0 (最大) に固定して全オブジェクトの背後に描画する
    // w で除算後の z = w/w = 1.0 になる
    gl_Position = clipPos.xyww;
}
