#include "GhostRecorder.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>

namespace Revora {

void GhostRecorder::StartRecording()
{
    frames_.clear();
    isRecording_    = true;
    lastRecordTime_ = -1.0f;
}

void GhostRecorder::RecordFrame(float raceTime, const Vector3& position,
                                 const Quaternion& rotation, float steerAngle,
                                 float speed)
{
    if (!isRecording_) {
        return;
    }

    // サンプリング間隔に達していなければスキップ
    if (lastRecordTime_ >= 0.0f &&
        (raceTime - lastRecordTime_) < kSampleInterval) {
        return;
    }

    GhostFrame frame;
    frame.timestamp  = raceTime;
    frame.position   = position;
    frame.rotation   = rotation;
    frame.steerAngle = steerAngle;
    frame.speed      = speed;

    frames_.push_back(frame);
    lastRecordTime_ = raceTime;
}

void GhostRecorder::StopRecording()
{
    isRecording_ = false;
}

GhostPlaybackState GhostRecorder::Sample(float time) const
{
    GhostPlaybackState state;

    if (frames_.empty()) {
        return state;
    }

    // 範囲外: 最初のフレーム以前
    if (time <= frames_.front().timestamp) {
        const GhostFrame& f = frames_.front();
        state.position   = f.position;
        state.rotation   = f.rotation;
        state.steerAngle = f.steerAngle;
        state.speed      = f.speed;
        state.isValid    = true;
        return state;
    }

    // 範囲外: 最後のフレーム以降
    if (time >= frames_.back().timestamp) {
        const GhostFrame& f = frames_.back();
        state.position   = f.position;
        state.rotation   = f.rotation;
        state.steerAngle = f.steerAngle;
        state.speed      = f.speed;
        state.isValid    = true;
        return state;
    }

    // 二分探索で time 以下の最大フレームを特定
    // lower_bound は timestamp > time の最初の要素を返す
    auto it = std::lower_bound(
        frames_.begin(), frames_.end(), time,
        [](const GhostFrame& frame, float t) {
            return frame.timestamp < t;
        });

    // it は time 以上の最初のフレーム。1つ前が time 以下の最大フレーム
    size_t idx = static_cast<size_t>(it - frames_.begin());
    if (idx == 0) {
        idx = 1;
    }

    const GhostFrame& a = frames_[idx - 1];
    const GhostFrame& b = frames_[idx];

    // 2フレーム間の補間係数を算出
    float duration = b.timestamp - a.timestamp;
    float t = (duration > 0.0f) ? (time - a.timestamp) / duration : 0.0f;

    state.position   = Vector3::Lerp(a.position, b.position, t);
    state.rotation   = Quaternion::Slerp(a.rotation, b.rotation, t);
    state.steerAngle = a.steerAngle + (b.steerAngle - a.steerAngle) * t;
    state.speed      = a.speed + (b.speed - a.speed) * t;
    state.isValid    = true;

    return state;
}

bool GhostRecorder::SaveToFile(const std::string& path) const
{
    if (frames_.empty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Ghost save skipped: no recorded frames");
        return false;
    }

    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open ghost file for writing: %s", path.c_str());
        return false;
    }

    // ヘッダー: マジックナンバー + バージョン + フレーム数
    uint32_t magic      = kMagic;
    uint32_t version    = kVersion;
    uint32_t frameCount = static_cast<uint32_t>(frames_.size());

    std::fwrite(&magic, sizeof(magic), 1, file);
    std::fwrite(&version, sizeof(version), 1, file);
    std::fwrite(&frameCount, sizeof(frameCount), 1, file);

    // フレームデータ一括書き込み (POD 保証済み)
    std::fwrite(frames_.data(), sizeof(GhostFrame), frameCount, file);

    std::fclose(file);

    SDL_Log("Ghost saved: %u frames (%.1f s) -> %s",
            frameCount, GetDuration(), path.c_str());
    return true;
}

bool GhostRecorder::LoadFromFile(const std::string& path)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        SDL_Log("No ghost file found: %s (first run)", path.c_str());
        return false;
    }

    // ヘッダー検証
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t frameCount = 0;

    if (std::fread(&magic, sizeof(magic), 1, file) != 1 ||
        std::fread(&version, sizeof(version), 1, file) != 1 ||
        std::fread(&frameCount, sizeof(frameCount), 1, file) != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Ghost file header read failed: %s", path.c_str());
        std::fclose(file);
        return false;
    }

    if (magic != kMagic) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Invalid ghost file magic: 0x%08X (expected 0x%08X)",
                     magic, kMagic);
        std::fclose(file);
        return false;
    }

    if (version != kVersion) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unsupported ghost file version: %u (expected %u)",
                     version, kVersion);
        std::fclose(file);
        return false;
    }

    // フレームデータ一括読み込み
    frames_.resize(frameCount);
    size_t read = std::fread(frames_.data(), sizeof(GhostFrame), frameCount, file);
    std::fclose(file);

    if (read != frameCount) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Ghost frame read incomplete: %zu / %u", read, frameCount);
        frames_.clear();
        return false;
    }

    SDL_Log("Ghost loaded: %u frames (%.1f s) from %s",
            frameCount, GetDuration(), path.c_str());
    return true;
}

float GhostRecorder::GetDuration() const
{
    if (frames_.empty()) {
        return 0.0f;
    }
    return frames_.back().timestamp - frames_.front().timestamp;
}

void GhostRecorder::Reset()
{
    frames_.clear();
    isRecording_    = false;
    lastRecordTime_ = -1.0f;
}

} // namespace Revora
