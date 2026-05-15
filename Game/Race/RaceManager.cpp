#include "RaceManager.h"

#include <SDL.h>

#include <cmath>

namespace Revora {

void RaceManager::Initialize(const CatmullRomSpline* spline,
                               const std::vector<float>& checkpointTs,
                               int totalLaps)
{
    checkpoint_.Initialize(spline, checkpointTs, totalLaps);
    timer_.Initialize(totalLaps);

    state_            = RaceState::Countdown;
    countdownTimer_   = 0.0f;
    lastCountdownLog_ = -1;
}

void RaceManager::Update(float dt, const Vector3& vehiclePosition)
{
    switch (state_) {
    case RaceState::Countdown:
        UpdateCountdown(dt);
        break;
    case RaceState::Racing:
        UpdateRacing(dt, vehiclePosition);
        break;
    case RaceState::Finished:
        // 完了後は更新不要
        break;
    }
}

void RaceManager::Reset()
{
    checkpoint_.Reset();
    timer_.Reset();

    state_            = RaceState::Countdown;
    countdownTimer_   = 0.0f;
    lastCountdownLog_ = -1;
}

int RaceManager::GetCountdownSeconds() const
{
    float remaining = kCountdownDuration - countdownTimer_;
    return static_cast<int>(std::ceil(remaining));
}

void RaceManager::UpdateCountdown(float dt)
{
    countdownTimer_ += dt;

    // 残り秒数をログ出力 (秒の切り替わり時のみ)
    int seconds = GetCountdownSeconds();
    if (seconds != lastCountdownLog_ && seconds > 0) {
        SDL_Log("Countdown: %d", seconds);
        lastCountdownLog_ = seconds;
    }

    // カウントダウン完了 → Racing へ遷移
    if (countdownTimer_ >= kCountdownDuration) {
        state_ = RaceState::Racing;
        SDL_Log("GO!");
    }
}

void RaceManager::UpdateRacing(float dt, const Vector3& vehiclePosition)
{
    // チェックポイント/ラップ判定
    int prevLap = checkpoint_.GetCurrentLap();
    checkpoint_.Update(vehiclePosition);

    // ラップ完了時にタイムを記録
    if (checkpoint_.GetCurrentLap() > prevLap) {
        timer_.RecordLap();
    }

    // レース完了判定
    if (checkpoint_.IsRaceFinished()) {
        state_ = RaceState::Finished;
        SDL_Log("Race finished! Total time: %.3f s", timer_.GetTotalTime());
        return;
    }

    // レース中はタイマーを進める
    timer_.Update(dt);
}

} // namespace Revora
