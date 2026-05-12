#pragma once

#include <vector>

namespace Revora {

/// ラップタイム計測・記録
/// 各ラップの経過時間と最速ラップタイムを管理する
class LapTimer {
public:
    /// 初期化: 合計ラップ数を設定
    void Initialize(int totalLaps);

    /// 毎フレーム呼ばれる: 経過時間を加算
    void Update(float dt);

    /// 現在のラップが完了した時に呼ぶ
    /// ラップタイムを記録し、現在のラップタイマーをリセット
    void RecordLap();

    /// 全状態をリセットする
    void Reset();

    float GetCurrentLapTime() const { return currentLapTime_; }
    float GetTotalTime() const { return totalTime_; }

    /// 記録済みラップタイムの一覧
    const std::vector<float>& GetLapTimes() const { return lapTimes_; }

    /// 最速ラップタイム (記録がなければ 0)
    float GetBestLapTime() const;

private:
    float currentLapTime_ = 0.0f;
    float totalTime_      = 0.0f;
    int   totalLaps_      = 3;

    std::vector<float> lapTimes_;
};

} // namespace Revora
