#pragma once

#include <cstdint>
#include <vector>

namespace Revora {

/// SpriteRenderer の頂点フォーマットと一致する構造体
/// SpriteRenderer.h の SpriteVertex と同一レイアウト (32 bytes)
struct SpriteVertex {
    float position[2];  // スクリーン座標 (ピクセル)
    float texCoord[2];  // UV
    float color[4];     // RGBA
};

/// ビットマップフォントアトラスのメタデータとテキスト→頂点変換
/// モノスペースグリフのグリッド配置を前提に、文字列を SpriteVertex 配列に変換する。
/// テクスチャ内のグリフ位置を UV 座標で管理し、スケーリングと着色を頂点レベルで行う
class BitmapFont {
public:
    /// フォントアトラスのレイアウト情報を設定する
    /// @param atlasWidth   アトラス画像の幅 (ピクセル)
    /// @param atlasHeight  アトラス画像の高さ (ピクセル)
    /// @param glyphWidth   1グリフの幅 (ピクセル)
    /// @param glyphHeight  1グリフの高さ (ピクセル)
    /// @param columns      アトラスの列数
    void Initialize(uint32_t atlasWidth, uint32_t atlasHeight,
                    uint32_t glyphWidth, uint32_t glyphHeight,
                    uint32_t columns);

    /// テキストを SpriteVertex 配列に変換する (6頂点/文字 = 2三角形)
    /// @param text  描画する ASCII テキスト
    /// @param x, y  左上のスクリーン座標 (ピクセル)
    /// @param scale 表示倍率
    /// @param r, g, b, a  テキストカラー
    /// @param outVertices  頂点を追加する出力先
    void GenerateTextVertices(
        const char* text,
        float x, float y, float scale,
        float r, float g, float b, float a,
        std::vector<SpriteVertex>& outVertices) const;

    /// テキスト幅を計算する (ピクセル)
    float MeasureWidth(const char* text, float scale) const;

    /// 行高さを返す (ピクセル)
    float GetLineHeight(float scale) const;

    /// ソリッドカラー矩形を頂点配列に追加する
    /// アトラス最終セルの全白ピクセルを UV 源として利用し、
    /// 頂点カラーのみで着色する
    void GenerateQuadVertices(
        float x, float y, float width, float height,
        float r, float g, float b, float a,
        std::vector<SpriteVertex>& outVertices) const;

private:
    float atlasWidth_  = 1.0f;
    float atlasHeight_ = 1.0f;
    float glyphWidth_  = 1.0f;
    float glyphHeight_ = 1.0f;
    uint32_t columns_  = 1;

    // アトラス最終セルの UV 座標 (ソリッド矩形用)
    float solidU0_ = 0.0f;
    float solidV0_ = 0.0f;
    float solidU1_ = 1.0f;
    float solidV1_ = 1.0f;

    static constexpr int kFirstChar = 32;   // space
    static constexpr int kLastChar  = 126;  // ~
};

} // namespace Revora
