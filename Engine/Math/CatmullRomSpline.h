#pragma once

#include "Vector3.h"

#include <vector>

namespace Revora {

/// 閉ループ対応 Catmull-Rom スプライン
/// コース中心線やカメラパスなどの滑らかな曲線を表現する汎用数学クラス
/// 制御点列から C1 連続な補間曲線を生成し、位置・接線・法線の評価と
/// 累積弧長テーブルによる等速パラメトリゼーションを提供する
class CatmullRomSpline {
public:
    /// 制御点列から閉ループスプラインを構築する
    /// 累積弧長テーブルも同時に構築される
    void Build(const std::vector<Vector3>& controlPoints);

    /// パラメータ t (0.0 ~ セグメント数) における位置を返す
    /// 閉ループなので t はセグメント数でラップする
    Vector3 Evaluate(float t) const;

    /// パラメータ t における接線ベクトル (正規化済み) を返す
    Vector3 EvaluateTangent(float t) const;

    /// パラメータ t における法線ベクトル (XZ 平面上の右方向) を返す
    /// 接線を Y 軸周りに 90 度回転させた水平方向ベクトル
    Vector3 EvaluateNormal(float t) const;

    /// ワールド座標の点から最も近いスプライン上の点を探索する
    /// 累積弧長テーブルで粗探索 → ニュートン法で精密化
    /// 戻り値: 最近点のパラメータ t
    float FindClosestParameter(const Vector3& point) const;

    /// 累積弧長テーブルからスプラインの全長を返す
    float GetTotalLength() const;

    /// セグメント数 (= 制御点数、閉ループ) を返す
    int GetSegmentCount() const { return segmentCount_; }

    /// 制御点の取得
    const std::vector<Vector3>& GetControlPoints() const { return points_; }

private:
    /// 4 点による Catmull-Rom 補間
    /// p0,p1,p2,p3 が隣接する制御点、t は 0~1 のローカルパラメータ
    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1,
                              const Vector3& p2, const Vector3& p3, float t);

    /// 4 点による Catmull-Rom 接線 (導関数)
    static Vector3 CatmullRomDerivative(const Vector3& p0, const Vector3& p1,
                                         const Vector3& p2, const Vector3& p3, float t);

    /// パラメータ t をセグメント範囲にラップし、セグメントインデックスとローカル t を返す
    void WrapParameter(float t, int& outSegment, float& outLocalT) const;

    /// 累積弧長テーブルを構築する
    void BuildArcLengthTable();

    std::vector<Vector3> points_;
    int segmentCount_ = 0;

    // 累積弧長テーブル: arcLengthTable_[i] は先頭からサンプル i までの累積距離
    std::vector<float> arcLengthTable_;

    // 弧長テーブルのサンプル数 (セグメントあたり)
    static constexpr int kSamplesPerSegment = 32;
};

} // namespace Revora
