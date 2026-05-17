#!/usr/bin/env python3
"""プレースホルダ音声ファイル生成スクリプト

Python 標準の wave モジュールだけで SE 5種 + BGM 1曲の WAV ファイルを生成する。
実際のゲーム開発ではサウンドデザイナーが差し替えることを前提とした仮素材。
"""

import wave
import struct
import math
import os

SAMPLE_RATE = 44100
NUM_CHANNELS = 1
SAMPLE_WIDTH = 2  # 16-bit
MAX_AMPLITUDE = 32767

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "Assets", "Audio")


def write_wav(filename: str, samples: list[int]) -> None:
    """16-bit モノラル WAV ファイルを書き出す"""
    filepath = os.path.join(OUTPUT_DIR, filename)
    with wave.open(filepath, "w") as wf:
        wf.setnchannels(NUM_CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        data = struct.pack(f"<{len(samples)}h", *samples)
        wf.writeframes(data)
    print(f"  Generated: {filepath}")


def generate_sine(freq: float, duration: float, amplitude: float = 1.0) -> list[int]:
    """単一周波数のサイン波を生成する"""
    n_samples = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        value = amplitude * math.sin(2.0 * math.pi * freq * t)
        samples.append(int(value * MAX_AMPLITUDE))
    return samples


def apply_envelope(samples: list[int], attack: float, decay: float) -> list[int]:
    """アタック/ディケイ エンベロープを適用する"""
    n_samples = len(samples)
    attack_samples = int(SAMPLE_RATE * attack)
    decay_samples = int(SAMPLE_RATE * decay)
    result = []
    for i in range(n_samples):
        envelope = 1.0
        if i < attack_samples:
            envelope = i / max(attack_samples, 1)
        elif i > n_samples - decay_samples:
            remaining = n_samples - i
            envelope = remaining / max(decay_samples, 1)
        result.append(int(samples[i] * envelope))
    return result


def generate_countdown_beep() -> None:
    """カウントダウンビープ: 440Hz, 0.15秒"""
    samples = generate_sine(440.0, 0.15, 0.7)
    samples = apply_envelope(samples, 0.005, 0.03)
    write_wav("countdown_beep.wav", samples)


def generate_go_beep() -> None:
    """GO! ビープ: 880Hz, 0.3秒"""
    samples = generate_sine(880.0, 0.3, 0.8)
    samples = apply_envelope(samples, 0.005, 0.1)
    write_wav("go_beep.wav", samples)


def generate_collision() -> None:
    """壁衝突音: ノイズ + 急速減衰, 0.2秒"""
    import random
    random.seed(42)
    n_samples = int(SAMPLE_RATE * 0.2)
    samples = []
    for i in range(n_samples):
        t = i / n_samples
        decay = math.exp(-t * 12.0)
        noise = random.uniform(-1.0, 1.0) * decay * 0.8
        samples.append(int(noise * MAX_AMPLITUDE))
    write_wav("collision.wav", samples)


def generate_lap_complete() -> None:
    """ラップ完了: 上昇音 (440→880Hz), 0.3秒"""
    n_samples = int(SAMPLE_RATE * 0.3)
    samples = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        progress = i / n_samples
        freq = 440.0 + (880.0 - 440.0) * progress
        value = 0.7 * math.sin(2.0 * math.pi * freq * t)
        samples.append(int(value * MAX_AMPLITUDE))
    samples = apply_envelope(samples, 0.005, 0.05)
    write_wav("lap_complete.wav", samples)


def generate_race_finish() -> None:
    """レース完了: 複合和音 (C5+E5+G5), 0.5秒"""
    n_samples = int(SAMPLE_RATE * 0.5)
    freqs = [523.25, 659.25, 783.99]  # C5, E5, G5
    samples = []
    for i in range(n_samples):
        t = i / SAMPLE_RATE
        value = 0.0
        for freq in freqs:
            value += math.sin(2.0 * math.pi * freq * t)
        value = value / len(freqs) * 0.7
        samples.append(int(value * MAX_AMPLITUDE))
    samples = apply_envelope(samples, 0.01, 0.15)
    write_wav("race_finish.wav", samples)


def generate_bgm() -> None:
    """BGM: シンプルなリズムパターン, 8秒ループ"""
    duration = 8.0
    n_samples = int(SAMPLE_RATE * duration)
    bpm = 120
    beat_duration = 60.0 / bpm
    samples = [0] * n_samples

    for i in range(n_samples):
        t = i / SAMPLE_RATE
        beat_pos = (t % beat_duration) / beat_duration

        # ベースライン: 4拍周期で低音を鳴らす
        bar_t = t % (beat_duration * 4)
        bass_notes = [110.0, 110.0, 146.83, 130.81]
        bass_idx = int(bar_t / beat_duration)
        bass_freq = bass_notes[min(bass_idx, 3)]
        bass = 0.3 * math.sin(2.0 * math.pi * bass_freq * t)

        # キック: 拍頭に短い低周波バースト
        kick = 0.0
        if beat_pos < 0.1:
            kick_env = 1.0 - beat_pos / 0.1
            kick = 0.4 * kick_env * math.sin(2.0 * math.pi * 60.0 * t)

        # ハイハット: 8分音符でノイズ
        eighth_pos = (t % (beat_duration / 2)) / (beat_duration / 2)
        hihat = 0.0
        if eighth_pos < 0.05:
            import random
            random.seed(i)
            hihat = 0.15 * random.uniform(-1.0, 1.0) * (1.0 - eighth_pos / 0.05)

        value = bass + kick + hihat
        value = max(-1.0, min(1.0, value))
        samples[i] = int(value * MAX_AMPLITUDE)

    write_wav("bgm_race.wav", samples)


def main() -> None:
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print("Generating placeholder sound files...")

    generate_countdown_beep()
    generate_go_beep()
    generate_collision()
    generate_lap_complete()
    generate_race_finish()
    generate_bgm()

    print("Done!")


if __name__ == "__main__":
    main()
