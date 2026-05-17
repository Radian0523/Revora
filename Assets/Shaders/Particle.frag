#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // 中心からの距離でフェードする円形グラデーション
    // ハードエッジのないソフトなパーティクル表現を実現する
    vec2 centered = fragUV * 2.0 - 1.0;
    float dist = dot(centered, centered);

    // 円の外側をカット (dist > 1.0 でフラグメントを破棄)
    if (dist > 1.0) {
        discard;
    }

    // smoothstep で中心から外周に向けてフェードアウト
    float fade = 1.0 - smoothstep(0.0, 1.0, dist);

    outColor = vec4(fragColor, fragAlpha * fade);
}
