#include "CourseMeshGenerator.h"

namespace Revora {

void CourseMeshGenerator::Generate(const CatmullRomSpline& spline,
                                    float trackWidth,
                                    std::vector<Vertex>& outVertices,
                                    std::vector<uint32_t>& outIndices)
{
    outVertices.clear();
    outIndices.clear();

    int segmentCount = spline.GetSegmentCount();
    if (segmentCount == 0) {
        return;
    }

    int totalSteps = segmentCount * kSubdivisionsPerSegment;

    // 路面は Y 軸上向きの平面: 法線は常に (0, 1, 0)
    float normal[3] = {0.0f, 1.0f, 0.0f};

    // 各分割点で左右の頂点を生成して帯状メッシュを構築
    for (int i = 0; i <= totalSteps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSubdivisionsPerSegment);
        float fraction = static_cast<float>(i) / static_cast<float>(totalSteps);

        Vector3 center = spline.Evaluate(t);
        Vector3 right  = spline.EvaluateNormal(t);

        // 中心線から左右に trackWidth だけオフセット
        Vector3 leftPos  = center - right * trackWidth;
        Vector3 rightPos = center + right * trackWidth;

        // UV: U は左端 0.0 ~ 右端 1.0、V はコース進行方向
        float v = fraction * static_cast<float>(segmentCount) * kUVRepeatV;

        // 路面を y=0 より僅かに浮かせてZファイティングを防ぐ
        float surfaceY = center.y + 0.01f;

        Vertex leftVert = {};
        leftVert.position[0] = leftPos.x;
        leftVert.position[1] = surfaceY;
        leftVert.position[2] = leftPos.z;
        leftVert.normal[0]   = normal[0];
        leftVert.normal[1]   = normal[1];
        leftVert.normal[2]   = normal[2];
        leftVert.texCoord[0] = 0.0f;
        leftVert.texCoord[1] = v;

        Vertex rightVert = {};
        rightVert.position[0] = rightPos.x;
        rightVert.position[1] = surfaceY;
        rightVert.position[2] = rightPos.z;
        rightVert.normal[0]   = normal[0];
        rightVert.normal[1]   = normal[1];
        rightVert.normal[2]   = normal[2];
        rightVert.texCoord[0] = 1.0f;
        rightVert.texCoord[1] = v;

        outVertices.push_back(leftVert);
        outVertices.push_back(rightVert);
    }

    // 三角形リストのインデックスを生成
    // 各分割ステップで左右 2 頂点 → 隣接ステップと合わせて 2 三角形 (= 1 クワッド)
    for (int i = 0; i < totalSteps; ++i) {
        uint32_t baseIdx = static_cast<uint32_t>(i * 2);

        // 閉ループの最終ステップ: 末端を先頭に接続
        uint32_t nextBase;
        if (i == totalSteps - 1) {
            nextBase = 0;
        }
        else {
            nextBase = baseIdx + 2;
        }

        // 三角形 1: 左下 → 右下 → 左上
        outIndices.push_back(baseIdx);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(nextBase);

        // 三角形 2: 右下 → 右上 → 左上
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(nextBase + 1);
        outIndices.push_back(nextBase);
    }
}

} // namespace Revora
