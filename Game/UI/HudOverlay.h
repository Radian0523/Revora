#pragma once

#include "BitmapFont.h"

#include <vector>

namespace Revora {

class GameFlowManager;
class RaceManager;
class VehiclePhysics;
struct CourseData;
struct RigidBody;

/// 全 HUD 要素の構成と頂点生成を担当する Game 層クラス
/// GameFlowManager の状態に応じて表示内容を切り替え、
/// BitmapFont を使って毎フレームの UI 頂点データを構築する。
/// 描画自体は SpriteRenderer に委譲する
class HudOverlay {
public:
    /// BitmapFont のレイアウト情報を設定する
    void Initialize(uint32_t fontAtlasWidth, uint32_t fontAtlasHeight,
                    uint32_t glyphWidth, uint32_t glyphHeight,
                    uint32_t columns);

    /// 毎フレーム: 現在のゲーム状態に応じた全 UI 要素の頂点を生成する
    /// @param fps 現在のフレームレート (FPS カウンター表示用)
    void BuildVertices(
        const GameFlowManager& flow,
        const RaceManager& race,
        const VehiclePhysics& physics,
        const CourseData& course,
        const RigidBody& vehicleBody,
        float fps,
        std::vector<SpriteVertex>& outVertices);

private:
    /// カウントダウン表示 (3, 2, 1, GO!)
    void BuildCountdown(const RaceManager& race,
                        float totalTime,
                        std::vector<SpriteVertex>& outVertices);

    /// レース中 HUD (ラップ、タイム、速度、ミニマップ)
    void BuildRacingHud(const RaceManager& race,
                        const VehiclePhysics& physics,
                        const CourseData& course,
                        const RigidBody& vehicleBody,
                        std::vector<SpriteVertex>& outVertices);

    /// ミニマップ描画
    void BuildMinimap(const CourseData& course,
                      const RigidBody& vehicleBody,
                      std::vector<SpriteVertex>& outVertices);

    /// リザルト画面
    void BuildResult(const RaceManager& race,
                     std::vector<SpriteVertex>& outVertices);

    /// FPS カウンター (左下隅に灰色で小さく表示)
    void BuildFPSCounter(float fps,
                         std::vector<SpriteVertex>& outVertices);

    /// float 秒 → "MM:SS.mmm" フォーマット
    static void FormatTime(float seconds, char* buffer, std::size_t bufferSize);

    BitmapFont font_;

    // 画面サイズ (レイアウト計算の基準)
    static constexpr float kScreenWidth  = 1280.0f;
    static constexpr float kScreenHeight = 720.0f;

    // ミニマップ設定
    static constexpr float kMinimapX      = 1080.0f;
    static constexpr float kMinimapY      = 20.0f;
    static constexpr float kMinimapSize   = 180.0f;
    static constexpr int   kMinimapSamples = 64;

    // m/s → km/h 変換係数
    static constexpr float kMsToKmh = 3.6f;
};

} // namespace Revora
