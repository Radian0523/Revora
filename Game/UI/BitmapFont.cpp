#include "BitmapFont.h"

#include <cstring>

namespace Revora {

void BitmapFont::Initialize(uint32_t atlasWidth, uint32_t atlasHeight,
                             uint32_t glyphWidth, uint32_t glyphHeight,
                             uint32_t columns)
{
    atlasWidth_  = static_cast<float>(atlasWidth);
    atlasHeight_ = static_cast<float>(atlasHeight);
    glyphWidth_  = static_cast<float>(glyphWidth);
    glyphHeight_ = static_cast<float>(glyphHeight);
    columns_     = columns;

    // 最終セル (index 95) のソリッド白ピクセル領域を UV 座標として記録
    // ASCII 32-126 = 95 文字なので、index 95 は次のセル
    uint32_t solidCol = 95 % columns;
    uint32_t solidRow = 95 / columns;
    solidU0_ = static_cast<float>(solidCol * glyphWidth) / atlasWidth_;
    solidV0_ = static_cast<float>(solidRow * glyphHeight) / atlasHeight_;
    solidU1_ = solidU0_ + glyphWidth_ / atlasWidth_;
    solidV1_ = solidV0_ + glyphHeight_ / atlasHeight_;
}

void BitmapFont::GenerateTextVertices(
    const char* text,
    float x, float y, float scale,
    float r, float g, float b, float a,
    std::vector<SpriteVertex>& outVertices) const
{
    if (!text)
    {
        return;
    }

    float cursorX = x;
    float scaledW = glyphWidth_ * scale;
    float scaledH = glyphHeight_ * scale;

    for (const char* p = text; *p != '\0'; ++p)
    {
        int ch = static_cast<unsigned char>(*p);

        // 印字不可能な文字はスペースと同じ幅だけ進める
        if (ch < kFirstChar || ch > kLastChar)
        {
            cursorX += scaledW;
            continue;
        }

        // グリフの UV 座標を計算
        int charIndex = ch - kFirstChar;
        int col = charIndex % static_cast<int>(columns_);
        int row = charIndex / static_cast<int>(columns_);

        float u0 = static_cast<float>(col) * glyphWidth_ / atlasWidth_;
        float v0 = static_cast<float>(row) * glyphHeight_ / atlasHeight_;
        float u1 = u0 + glyphWidth_ / atlasWidth_;
        float v1 = v0 + glyphHeight_ / atlasHeight_;

        // 2 三角形 = 6 頂点 (左上, 右上, 左下, 左下, 右上, 右下)
        float x0 = cursorX;
        float y0 = y;
        float x1 = cursorX + scaledW;
        float y1 = y + scaledH;

        SpriteVertex verts[6] = {
            {{x0, y0}, {u0, v0}, {r, g, b, a}},
            {{x1, y0}, {u1, v0}, {r, g, b, a}},
            {{x0, y1}, {u0, v1}, {r, g, b, a}},
            {{x0, y1}, {u0, v1}, {r, g, b, a}},
            {{x1, y0}, {u1, v0}, {r, g, b, a}},
            {{x1, y1}, {u1, v1}, {r, g, b, a}},
        };

        outVertices.insert(outVertices.end(), verts, verts + 6);
        cursorX += scaledW;
    }
}

float BitmapFont::MeasureWidth(const char* text, float scale) const
{
    if (!text)
    {
        return 0.0f;
    }
    return static_cast<float>(std::strlen(text)) * glyphWidth_ * scale;
}

float BitmapFont::GetLineHeight(float scale) const
{
    return glyphHeight_ * scale;
}

void BitmapFont::GenerateQuadVertices(
    float x, float y, float width, float height,
    float r, float g, float b, float a,
    std::vector<SpriteVertex>& outVertices) const
{
    float x0 = x;
    float y0 = y;
    float x1 = x + width;
    float y1 = y + height;

    // ソリッド白ピクセルの中央 UV を使用 (テクスチャフィルタリングで端が透明にならないよう)
    float uMid = (solidU0_ + solidU1_) * 0.5f;
    float vMid = (solidV0_ + solidV1_) * 0.5f;

    SpriteVertex verts[6] = {
        {{x0, y0}, {uMid, vMid}, {r, g, b, a}},
        {{x1, y0}, {uMid, vMid}, {r, g, b, a}},
        {{x0, y1}, {uMid, vMid}, {r, g, b, a}},
        {{x0, y1}, {uMid, vMid}, {r, g, b, a}},
        {{x1, y0}, {uMid, vMid}, {r, g, b, a}},
        {{x1, y1}, {uMid, vMid}, {r, g, b, a}},
    };

    outVertices.insert(outVertices.end(), verts, verts + 6);
}

} // namespace Revora
