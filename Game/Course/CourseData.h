#pragma once

#include "../../Engine/Math/Vector3.h"
#include "../../Engine/Math/CatmullRomSpline.h"

#include <string>
#include <vector>

namespace Revora {

/// コース定義データ
/// JSON ファイルから読み込まれるデータドリブン構造体
/// 制御点を変更するだけで異なるコース形状を生成できる
struct CourseData {
    std::vector<Vector3> controlPoints;   // スプライン制御点
    float trackWidth    = 10.0f;          // トラック幅 (片側, 中心線からの距離)
    int   lapCount      = 3;              // 完走ラップ数

    // チェックポイント: スプラインパラメータ t の位置
    // controlPoints.size() 個のセグメントがあり、t はセグメント番号に対応
    std::vector<float> checkpointPositions;

    // 車両のスポーン位置: スプラインパラメータ t
    float spawnT = 0.0f;

    /// JSON ファイルからコースデータを読み込む
    /// ファイルが存在しない場合はデフォルトの楕円コースを生成する
    bool LoadFromFile(const std::string& filepath);

    /// スプラインを構築する (LoadFromFile 後に呼ぶ)
    void BuildSpline();

    /// 構築済みスプラインへのアクセス
    const CatmullRomSpline& GetSpline() const { return spline_; }

private:
    CatmullRomSpline spline_;
};

} // namespace Revora
