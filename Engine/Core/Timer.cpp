#include "Timer.h"
#include <SDL.h>

namespace Revora {

void Timer::Initialize() {
    frequency_     = SDL_GetPerformanceFrequency();
    previousCount_ = SDL_GetPerformanceCounter();
    deltaTime_     = 0.0f;
    totalTime_     = 0.0f;
    fpsAccumulator_= 0.0f;
    frameCount_    = 0;
    fps_           = 0.0f;
}

void Timer::Tick() {
    uint64_t currentCount = SDL_GetPerformanceCounter();
    uint64_t elapsed = currentCount - previousCount_;
    previousCount_ = currentCount;

    deltaTime_ = static_cast<float>(elapsed) / static_cast<float>(frequency_);
    totalTime_ += deltaTime_;

    // FPS を 1 秒間隔で更新
    ++frameCount_;
    fpsAccumulator_ += deltaTime_;
    if (fpsAccumulator_ >= 1.0f) {
        fps_ = static_cast<float>(frameCount_) / fpsAccumulator_;
        frameCount_     = 0;
        fpsAccumulator_ = 0.0f;
    }
}

} // namespace Revora
