#define MINIAUDIO_IMPLEMENTATION
#include "AudioManager.h"

#include <SDL.h>

namespace Revora {

bool AudioManager::Initialize()
{
    ma_engine_config config = ma_engine_config_init();

    ma_result result = ma_engine_init(&config, &engine_);
    if (result != MA_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "AudioManager: Failed to initialize ma_engine (error %d)", result);
        return false;
    }

    engineReady_ = true;

    for (int i = 0; i < kMaxSESlots; ++i) {
        seLoaded_[i] = false;
    }
    bgmLoaded_ = false;

    SDL_Log("AudioManager: Initialized");
    return true;
}

void AudioManager::Shutdown()
{
    if (!engineReady_) {
        return;
    }

    // BGM 解放
    if (bgmLoaded_) {
        ma_sound_uninit(&bgmSound_);
        bgmLoaded_ = false;
    }

    // SE 解放
    for (int i = 0; i < kMaxSESlots; ++i) {
        if (seLoaded_[i]) {
            ma_sound_uninit(&seSounds_[i]);
            seLoaded_[i] = false;
        }
    }

    ma_engine_uninit(&engine_);
    engineReady_ = false;

    SDL_Log("AudioManager: Shutdown");
}

// --- BGM ---

bool AudioManager::LoadBGM(const char* filePath)
{
    if (!engineReady_) {
        return false;
    }

    // 既存の BGM があれば解放する
    if (bgmLoaded_) {
        ma_sound_uninit(&bgmSound_);
        bgmLoaded_ = false;
    }

    // MA_SOUND_FLAG_NO_SPATIALIZATION: BGM は空間音響不要
    ma_result result = ma_sound_init_from_file(
        &engine_, filePath, MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr, nullptr, &bgmSound_);

    if (result != MA_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "AudioManager: Failed to load BGM '%s' (error %d)", filePath, result);
        return false;
    }

    ma_sound_set_looping(&bgmSound_, MA_TRUE);
    ma_sound_set_volume(&bgmSound_, bgmVolume_);
    bgmLoaded_ = true;
    return true;
}

void AudioManager::PlayBGM()
{
    if (!bgmLoaded_) {
        return;
    }

    // 先頭に戻してから再生 (リトライ時にも対応)
    ma_sound_seek_to_pcm_frame(&bgmSound_, 0);
    ma_sound_start(&bgmSound_);
}

void AudioManager::StopBGM()
{
    if (!bgmLoaded_) {
        return;
    }
    ma_sound_stop(&bgmSound_);
}

void AudioManager::SetBGMVolume(float volume)
{
    bgmVolume_ = volume;
    if (bgmLoaded_) {
        ma_sound_set_volume(&bgmSound_, volume);
    }
}

// --- SE ---

bool AudioManager::LoadSE(int slot, const char* filePath)
{
    if (!engineReady_ || slot < 0 || slot >= kMaxSESlots) {
        return false;
    }

    if (seLoaded_[slot]) {
        ma_sound_uninit(&seSounds_[slot]);
        seLoaded_[slot] = false;
    }

    ma_result result = ma_sound_init_from_file(
        &engine_, filePath, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_DECODE,
        nullptr, nullptr, &seSounds_[slot]);

    if (result != MA_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "AudioManager: Failed to load SE slot %d '%s' (error %d)",
                     slot, filePath, result);
        return false;
    }

    ma_sound_set_volume(&seSounds_[slot], seVolume_);
    seLoaded_[slot] = true;
    return true;
}

void AudioManager::PlaySE(int slot)
{
    if (slot < 0 || slot >= kMaxSESlots || !seLoaded_[slot]) {
        return;
    }

    // 再生中でも先頭に巻き戻して即時再生する
    // (同一フレーム内で複数回トリガーされた場合は最後のトリガーが優先される)
    ma_sound_seek_to_pcm_frame(&seSounds_[slot], 0);
    ma_sound_start(&seSounds_[slot]);
}

void AudioManager::SetSEVolume(float volume)
{
    seVolume_ = volume;
    for (int i = 0; i < kMaxSESlots; ++i) {
        if (seLoaded_[i]) {
            ma_sound_set_volume(&seSounds_[i], volume);
        }
    }
}

void AudioManager::StopAll()
{
    StopBGM();
    for (int i = 0; i < kMaxSESlots; ++i) {
        if (seLoaded_[i]) {
            ma_sound_stop(&seSounds_[i]);
        }
    }
}

} // namespace Revora
