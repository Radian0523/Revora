#version 450

// UBO: カメラ行列
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera;

// パーティクルごとのインスタンスデータ (頂点バッファとして渡す)
layout(location = 0) in vec3 inPosition;   // ワールド空間の中心位置
layout(location = 1) in float inSize;      // パーティクルサイズ
layout(location = 2) in float inAlpha;     // 透明度
layout(location = 3) in vec3 inColor;      // パーティクル色

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragAlpha;
layout(location = 2) out vec3 fragColor;

void main() {
    // gl_VertexIndex 0..5 から三角形 2 枚のクアッドを展開
    // 0--1    三角形 1: 0,1,2
    // |\ |    三角形 2: 2,1,3
    // | \|
    // 2--3
    const vec2 offsets[6] = vec2[](
        vec2(-0.5, -0.5),  // 0: 左上
        vec2( 0.5, -0.5),  // 1: 右上
        vec2(-0.5,  0.5),  // 2: 左下
        vec2(-0.5,  0.5),  // 2: 左下
        vec2( 0.5, -0.5),  // 1: 右上
        vec2( 0.5,  0.5)   // 3: 右下
    );

    const vec2 uvs[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    vec2 offset = offsets[gl_VertexIndex % 6];

    // ビルボード: カメラの right/up 方向にクアッドを展開
    // ビュー行列の上 3x3 から right (row 0) と up (row 1) を取得
    vec3 cameraRight = vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    vec3 cameraUp    = vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]);

    vec3 worldPos = inPosition
                  + cameraRight * offset.x * inSize
                  + cameraUp    * offset.y * inSize;

    gl_Position = camera.projection * camera.view * vec4(worldPos, 1.0);

    fragUV    = uvs[gl_VertexIndex % 6];
    fragAlpha = inAlpha;
    fragColor = inColor;
}
