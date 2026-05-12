#pragma once

#include <cstdint>

namespace Revora {

class Timer {
public:
    Timer() = default;

    void Initialize();
    void Tick();

    float GetDeltaTime() const   { return deltaTime_; }
    float GetTotalTime() const   { return totalTime_; }
    float GetFPS() const         { return fps_; }

private:
    uint64_t frequency_      = 0;
    uint64_t previousCount_  = 0;
    float    deltaTime_      = 0.0f;
    float    totalTime_      = 0.0f;

    // FPS 計測用
    float    fpsAccumulator_ = 0.0f;
    int      frameCount_     = 0;
    float    fps_            = 0.0f;
};

} // namespace Revora
