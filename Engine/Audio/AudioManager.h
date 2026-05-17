#pragma once

#include <cstdint>

// miniaudio の型を前方参照するためにフルインクルードが必要
// (pimpl では隠蔽しない: ポートフォリオとして内部構造を明示するため)
#include <miniaudio.h>

namespace Revora {

/// SE スロットのインデックス定義
/// Application 側でトリガーに応じて再生するスロットを指定する
enum SESlot : int {
    kSECountdownBeep = 0,
    kSEGoBeep        = 1,
    kSECollision     = 2,
    kSELapComplete   = 3,
    kSERaceFinish    = 4,
    kSESlotCount     = 5
};

/// miniaudio のハイレベル API (ma_engine) をラップするオーディオ管理クラス
///
/// 設計判断:
/// - ma_engine を使用することで、デコード・ミキシング・リサンプリングを
///   miniaudio に委譲し、実装コストを最小化する
/// - BGM は 1 トラック固定 (レースゲームでは BGM の同時再生は不要)
/// - SE はスロット方式で管理し、同一フレーム内の多重トリガーを防ぐ。
///   スロット数は固定 (8) で、用途ごとにインデックスで管理する
class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() = default;

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    /// miniaudio エンジンを初期化する
    bool Initialize();

    /// 全リソースを解放する
    void Shutdown();

    // --- BGM ---

    /// BGM ファイルをロードする (WAV 形式)
    bool LoadBGM(const char* filePath);

    /// BGM をループ再生する
    void PlayBGM();

    /// BGM を停止する
    void StopBGM();

    /// BGM 音量を設定する (0.0 ~ 1.0)
    void SetBGMVolume(float volume);

    // --- SE ---

    /// SE ファイルをスロットにロードする
    bool LoadSE(int slot, const char* filePath);

    /// 指定スロットの SE をワンショット再生する
    /// 再生中の場合は先頭に巻き戻して再再生する
    void PlaySE(int slot);

    /// SE 全体の音量を設定する (0.0 ~ 1.0)
    void SetSEVolume(float volume);

    /// 全サウンドを停止する (リセット時に使用)
    void StopAll();

private:
    static constexpr int kMaxSESlots = 8;

    ma_engine engine_         = {};
    bool      engineReady_    = false;

    ma_sound  bgmSound_       = {};
    bool      bgmLoaded_      = false;

    ma_sound  seSounds_[kMaxSESlots] = {};
    bool      seLoaded_[kMaxSESlots] = {};

    float     bgmVolume_ = 0.5f;
    float     seVolume_  = 0.7f;
};

} // namespace Revora
