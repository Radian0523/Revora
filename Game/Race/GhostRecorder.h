#pragma once

#include "../../Engine/Math/Vector3.h"
#include "../../Engine/Math/Quaternion.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Revora {

/// ゴーストリプレイ用の1フレーム分のスナップショット
/// バイナリ I/O のため POD 型を保証する
struct GhostFrame {
    float      timestamp;   // レース開始からの経過時間 (秒)
    Vector3    position;
    Quaternion rotation;
    float      steerAngle;  // フロントホイールの操舵角 (ラジアン)
    float      speed;       // 車速 (m/s)
};
static_assert(std::is_trivially_copyable_v<GhostFrame>,
              "GhostFrame must be trivially copyable for binary I/O");

/// ゴースト車両の補間済み状態
/// 任意時刻における位置・回転を再生側に返す
struct GhostPlaybackState {
    Vector3    position;
    Quaternion rotation;
    float      steerAngle = 0.0f;
    float      speed      = 0.0f;
    bool       isValid    = false;  // 再生データが存在するか
};

/// ゴースト記録・再生・バイナリ I/O
/// 固定間隔で車両状態をサンプリングし、ファイルに保存する
/// 再生時は二分探索 + Lerp/Slerp 補間で任意時刻の状態を復元する
class GhostRecorder {
public:
    /// 記録を開始する (フレームバッファをクリア)
    void StartRecording();

    /// 固定タイムステップごとに呼ばれ、サンプリング間隔に達したらフレームを記録する
    void RecordFrame(float raceTime, const Vector3& position,
                     const Quaternion& rotation, float steerAngle, float speed);

    /// 記録を停止する
    void StopRecording();

    /// 任意時刻のゴースト状態を補間して返す
    /// 二分探索でフレームを特定し、Lerp/Slerp で補間する
    GhostPlaybackState Sample(float time) const;

    /// バイナリ形式でファイルに保存する
    /// フォーマット: [magic "RVGH"][version][frameCount][GhostFrame * N]
    bool SaveToFile(const std::string& path) const;

    /// バイナリ形式でファイルから読み込む
    bool LoadFromFile(const std::string& path);

    /// 再生用データが存在するか
    bool HasData() const { return !frames_.empty(); }

    /// 記録済みフレーム数
    size_t GetFrameCount() const { return frames_.size(); }

    /// ゴーストデータの総時間
    float GetDuration() const;

    /// 全状態をリセットする
    void Reset();

private:
    std::vector<GhostFrame> frames_;

    bool  isRecording_       = false;
    float lastRecordTime_    = -1.0f;

    // 1/30秒間隔でサンプリング (60FPS 物理の半分で十分な精度)
    static constexpr float kSampleInterval = 1.0f / 30.0f;

    // バイナリファイルヘッダー
    static constexpr uint32_t kMagic   = 0x48475652;  // "RVGH" (little-endian)
    static constexpr uint32_t kVersion = 1;
};

} // namespace Revora
