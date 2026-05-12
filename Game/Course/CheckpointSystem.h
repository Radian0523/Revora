#pragma once

#include "../../Engine/Math/CatmullRomSpline.h"
#include "../../Engine/Math/Vector3.h"

#include <vector>

namespace Revora {

/// チェックポイント通過判定 + ラップカウント管理
/// スプラインパラメータ上に配置されたチェックポイントを順番に通過することでラップを完了する
/// ショートカット防止のため、チェックポイントは順序通りに通過する必要がある
class CheckpointSystem {
public:
    /// チェックポイントの初期設定
    /// checkpointTs: スプラインパラメータ t のリスト (昇順)
    /// totalLaps: 完走に必要なラップ数
    void Initialize(const CatmullRomSpline* spline,
                    const std::vector<float>& checkpointTs,
                    int totalLaps);

    /// 車両位置を受け取り、チェックポイント通過を判定する
    /// ラップ完了・レース完了時は SDL_Log で出力する
    void Update(const Vector3& vehiclePosition);

    /// 状態をリセットする
    void Reset();

    int GetCurrentLap() const { return currentLap_; }
    int GetTotalLaps() const { return totalLaps_; }
    bool IsRaceFinished() const { return isRaceFinished_; }

    /// 次に通過すべきチェックポイントのインデックス
    int GetNextCheckpointIndex() const { return nextCheckpoint_; }

private:
    const CatmullRomSpline* spline_ = nullptr;
    std::vector<float> checkpointTs_;

    int nextCheckpoint_   = 0;
    int currentLap_       = 0;
    int totalLaps_        = 3;
    bool isRaceFinished_  = false;

    // チェックポイント判定の距離閾値 (中心線からの距離がこれ以下で通過)
    static constexpr float kCheckpointRadius = 12.0f;
};

} // namespace Revora
