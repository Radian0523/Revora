#pragma once

#include "../../Engine/Math/CatmullRomSpline.h"
#include "../../Engine/Renderer/Vertex.h"

#include <vector>

namespace Revora {

/// プロシージャルコースメッシュ生成器
/// スプライン中心線と幅から路面メッシュの頂点/インデックス配列を生成する
/// 生成された配列は MeshLoader::CreateMesh() で GPU にアップロードする
class CourseMeshGenerator {
public:
    /// スプラインとトラック幅から路面メッシュを生成する
    /// 閉ループの帯状メッシュを三角形リストとして出力する
    static void Generate(const CatmullRomSpline& spline,
                         float trackWidth,
                         std::vector<Vertex>& outVertices,
                         std::vector<uint32_t>& outIndices);

private:
    // セグメントあたりの分割数: 曲線の滑らかさに影響
    static constexpr int kSubdivisionsPerSegment = 16;

    // UV の縦方向リピート倍率: チェッカーテクスチャの見た目を調整
    static constexpr float kUVRepeatV = 2.0f;
};

} // namespace Revora
