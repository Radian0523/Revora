#include "LapTimer.h"

#include <SDL.h>

#include <algorithm>
#include <limits>

namespace Revora {

void LapTimer::Initialize(int totalLaps)
{
    totalLaps_ = totalLaps;
    Reset();
}

void LapTimer::Update(float dt)
{
    currentLapTime_ += dt;
    totalTime_ += dt;
}

void LapTimer::RecordLap()
{
    lapTimes_.push_back(currentLapTime_);

    SDL_Log("Lap time: %.3f s (best: %.3f s, total: %.3f s)",
            currentLapTime_, GetBestLapTime(), totalTime_);

    currentLapTime_ = 0.0f;
}

void LapTimer::Reset()
{
    currentLapTime_ = 0.0f;
    totalTime_      = 0.0f;
    lapTimes_.clear();
}

float LapTimer::GetBestLapTime() const
{
    if (lapTimes_.empty()) {
        return 0.0f;
    }

    float best = std::numeric_limits<float>::max();
    for (float t : lapTimes_) {
        best = std::min(best, t);
    }
    return best;
}

} // namespace Revora
