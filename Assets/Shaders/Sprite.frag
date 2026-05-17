#version 450

layout(set = 0, binding = 1) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // フォントアトラスのテクセルを取得
    // テキスト: 白グリフ × 頂点カラーで着色
    // ソリッド矩形: アトラス内の全白ピクセル領域を UV に指定して使用
    vec4 texel = texture(fontAtlas, fragTexCoord);

    outColor = vec4(fragColor.rgb * texel.rgb, fragColor.a * texel.a);
}
