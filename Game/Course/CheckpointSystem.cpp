#include "CheckpointSystem.h"
#include "../../Engine/Math/MathConstants.h"

#include <SDL.h>

#include <cmath>

namespace Revora {

void CheckpointSystem::Initialize(const CatmullRomSpline* spline,
                                   const std::vector<float>& checkpointTs,
                                   int totalLaps)
{
    spline_         = spline;
    checkpointTs_   = checkpointTs;
    totalLaps_      = totalLaps;
    nextCheckpoint_ = 0;
    currentLap_     = 0;
    isRaceFinished_ = false;
}

void CheckpointSystem::Update(const Vector3& vehiclePosition)
{
    if (isRaceFinished_ || !spline_ || checkpointTs_.empty()) {
        return;
    }

    // 次に通過すべきチェックポイントの位置を取得
    float cpT = checkpointTs_[nextCheckpoint_];
    Vector3 cpPos = spline_->Evaluate(cpT);

    // XZ 平面上の距離で判定
    float dx = vehiclePosition.x - cpPos.x;
    float dz = vehiclePosition.z - cpPos.z;
    float distSq = dx * dx + dz * dz;

    if (distSq > kCheckpointRadius * kCheckpointRadius) {
        return;
    }

    // チェックポイント通過
    SDL_Log("Checkpoint %d/%zu passed",
            nextCheckpoint_ + 1,
            checkpointTs_.size());

    nextCheckpoint_++;

    // 全チェックポイント通過 → ラップ完了
    if (nextCheckpoint_ >= static_cast<int>(checkpointTs_.size())) {
        nextCheckpoint_ = 0;
        currentLap_++;

        SDL_Log("Lap %d/%d completed!", currentLap_, totalLaps_);

        if (currentLap_ >= totalLaps_) {
            isRaceFinished_ = true;
            SDL_Log("Race finished!");
        }
    }
}

void CheckpointSystem::Reset()
{
    nextCheckpoint_ = 0;
    currentLap_     = 0;
    isRaceFinished_ = false;
}

} // namespace Revora
