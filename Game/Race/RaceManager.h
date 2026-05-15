#pragma once

#include "../Course/CheckpointSystem.h"
#include "../Course/LapTimer.h"

#include <vector>

namespace Revora {

class CatmullRomSpline;

/// レースの進行状態
enum class RaceState {
    Countdown,  // カウントダウン中 (車両操作不可)
    Racing,     // レース中
    Finished    // 全ラップ完了
};

/// レースのライフサイクルを一元管理する
/// CheckpointSystem と LapTimer を内部に所有し、
/// カウントダウン → レース → 完了 の状態遷移を自動で行う
class RaceManager {
public:
    /// レースの初期設定
    /// CheckpointSystem と LapTimer を内部で初期化する
    void Initialize(const CatmullRomSpline* spline,
                    const std::vector<float>& checkpointTs,
                    int totalLaps);

    /// 毎フレーム呼ばれる: カウントダウン / ラップ判定 / タイマー更新
    void Update(float dt, const Vector3& vehiclePosition);

    /// 全状態をリセットしてカウントダウンからやり直す
    void Reset();

    // --- 状態クエリ ---
    RaceState GetState() const { return state_; }
    bool IsInputEnabled() const { return state_ == RaceState::Racing; }

    // --- カウントダウン ---

    /// カウントダウンの残り秒数 (整数表示用)
    int GetCountdownSeconds() const;

    // --- CheckpointSystem 代理アクセサ ---
    int  GetCurrentLap() const { return checkpoint_.GetCurrentLap(); }
    int  GetTotalLaps() const { return checkpoint_.GetTotalLaps(); }
    bool IsRaceFinished() const { return checkpoint_.IsRaceFinished(); }
    int  GetNextCheckpointIndex() const { return checkpoint_.GetNextCheckpointIndex(); }

    // --- LapTimer 代理アクセサ ---
    float GetCurrentLapTime() const { return timer_.GetCurrentLapTime(); }
    float GetTotalTime() const { return timer_.GetTotalTime(); }
    float GetBestLapTime() const { return timer_.GetBestLapTime(); }
    const std::vector<float>& GetLapTimes() const { return timer_.GetLapTimes(); }

private:
    void UpdateCountdown(float dt);
    void UpdateRacing(float dt, const Vector3& vehiclePosition);

    CheckpointSystem checkpoint_;
    LapTimer         timer_;

    RaceState state_         = RaceState::Countdown;
    float countdownTimer_    = 0.0f;
    int   lastCountdownLog_  = -1;  // 重複ログ防止用

    // カウントダウンの長さ (秒)
    static constexpr float kCountdownDuration = 3.0f;
};

} // namespace Revora
