#include "CatmullRomSpline.h"
#include "MathConstants.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace Revora {

void CatmullRomSpline::Build(const std::vector<Vector3>& controlPoints)
{
    points_ = controlPoints;
    segmentCount_ = static_cast<int>(points_.size());
    BuildArcLengthTable();
}

Vector3 CatmullRomSpline::Evaluate(float t) const
{
    int seg;
    float localT;
    WrapParameter(t, seg, localT);

    int count = segmentCount_;
    const Vector3& p0 = points_[((seg - 1) % count + count) % count];
    const Vector3& p1 = points_[seg];
    const Vector3& p2 = points_[(seg + 1) % count];
    const Vector3& p3 = points_[(seg + 2) % count];

    return CatmullRom(p0, p1, p2, p3, localT);
}

Vector3 CatmullRomSpline::EvaluateTangent(float t) const
{
    int seg;
    float localT;
    WrapParameter(t, seg, localT);

    int count = segmentCount_;
    const Vector3& p0 = points_[((seg - 1) % count + count) % count];
    const Vector3& p1 = points_[seg];
    const Vector3& p2 = points_[(seg + 1) % count];
    const Vector3& p3 = points_[(seg + 2) % count];

    Vector3 tangent = CatmullRomDerivative(p0, p1, p2, p3, localT);
    float len = tangent.Length();
    if (len < kEpsilon) {
        return Vector3::Forward;
    }
    return tangent * (1.0f / len);
}

Vector3 CatmullRomSpline::EvaluateNormal(float t) const
{
    // コース法線は XZ 平面上で定義: 接線を Y 軸周りに 90 度回転
    // 左手座標系 (+Z 前方) で右方向を返す
    Vector3 tangent = EvaluateTangent(t);
    return Vector3(tangent.z, 0.0f, -tangent.x);
}

float CatmullRomSpline::FindClosestParameter(const Vector3& point) const
{
    if (segmentCount_ == 0) {
        return 0.0f;
    }

    int totalSamples = segmentCount_ * kSamplesPerSegment;

    // 粗探索: 弧長テーブルのサンプル点から最近点を見つける
    float bestDistSq = std::numeric_limits<float>::max();
    int bestIndex = 0;

    for (int i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSamplesPerSegment);
        Vector3 pos = Evaluate(t);
        Vector3 diff = pos - point;
        float distSq = diff.x * diff.x + diff.z * diff.z;  // XZ 平面上の距離
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = i;
        }
    }

    // 精密化: 最近サンプルの前後を二分探索で絞り込む
    float tBest = static_cast<float>(bestIndex) / static_cast<float>(kSamplesPerSegment);
    float tStep = 1.0f / static_cast<float>(kSamplesPerSegment);
    float tMin = tBest - tStep;
    float tMax = tBest + tStep;

    // 二分探索を数回繰り返して精度を上げる
    static constexpr int kRefinementIterations = 8;
    for (int iter = 0; iter < kRefinementIterations; ++iter) {
        float tMid = (tMin + tMax) * 0.5f;

        float tA = (tMin + tMid) * 0.5f;
        float tB = (tMid + tMax) * 0.5f;

        Vector3 posA = Evaluate(tA);
        Vector3 posB = Evaluate(tB);

        Vector3 diffA = posA - point;
        Vector3 diffB = posB - point;
        float distSqA = diffA.x * diffA.x + diffA.z * diffA.z;
        float distSqB = diffB.x * diffB.x + diffB.z * diffB.z;

        if (distSqA < distSqB) {
            tMax = tMid;
        }
        else {
            tMin = tMid;
        }
    }

    return (tMin + tMax) * 0.5f;
}

float CatmullRomSpline::GetTotalLength() const
{
    if (arcLengthTable_.empty()) {
        return 0.0f;
    }
    return arcLengthTable_.back();
}

Vector3 CatmullRomSpline::CatmullRom(const Vector3& p0, const Vector3& p1,
                                       const Vector3& p2, const Vector3& p3, float t)
{
    // 標準 Catmull-Rom 行列による補間:
    // q(t) = 0.5 * ((2*p1) + (-p0 + p2)*t + (2*p0 - 5*p1 + 4*p2 - p3)*t^2
    //              + (-p0 + 3*p1 - 3*p2 + p3)*t^3)
    float t2 = t * t;
    float t3 = t2 * t;

    return (p1 * 2.0f
          + (p2 - p0) * t
          + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
          + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

Vector3 CatmullRomSpline::CatmullRomDerivative(const Vector3& p0, const Vector3& p1,
                                                 const Vector3& p2, const Vector3& p3, float t)
{
    // q(t) の一階導関数:
    // q'(t) = 0.5 * ((-p0 + p2) + (4*p0 - 10*p1 + 8*p2 - 2*p3)*t
    //               + (-3*p0 + 9*p1 - 9*p2 + 3*p3)*t^2)
    float t2 = t * t;

    return ((p2 - p0)
          + (p0 * 4.0f - p1 * 10.0f + p2 * 8.0f - p3 * 2.0f) * t
          + (p1 * 9.0f - p0 * 3.0f - p2 * 9.0f + p3 * 3.0f) * t2) * 0.5f;
}

void CatmullRomSpline::WrapParameter(float t, int& outSegment, float& outLocalT) const
{
    // t を [0, segmentCount) の範囲にラップ
    float wrapped = std::fmod(t, static_cast<float>(segmentCount_));
    if (wrapped < 0.0f) {
        wrapped += static_cast<float>(segmentCount_);
    }

    outSegment = static_cast<int>(wrapped);
    if (outSegment >= segmentCount_) {
        outSegment = 0;
    }

    outLocalT = wrapped - static_cast<float>(outSegment);
}

void CatmullRomSpline::BuildArcLengthTable()
{
    int totalSamples = segmentCount_ * kSamplesPerSegment;
    arcLengthTable_.resize(totalSamples + 1);
    arcLengthTable_[0] = 0.0f;

    Vector3 prevPos = Evaluate(0.0f);
    for (int i = 1; i <= totalSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSamplesPerSegment);
        Vector3 pos = Evaluate(t);
        float segmentLength = (pos - prevPos).Length();
        arcLengthTable_[i] = arcLengthTable_[i - 1] + segmentLength;
        prevPos = pos;
    }
}

} // namespace Revora
