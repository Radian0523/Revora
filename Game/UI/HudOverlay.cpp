#include "HudOverlay.h"
#include "../Flow/GameFlowManager.h"
#include "../Race/RaceManager.h"
#include "../Course/CourseData.h"
#include "../../Engine/Physics/VehiclePhysics.h"
#include "../../Engine/Physics/RigidBody.h"
#include "../../Engine/Math/CatmullRomSpline.h"

#include <cstdio>
#include <cmath>
#include <algorithm>

namespace Revora {

void HudOverlay::Initialize(uint32_t fontAtlasWidth, uint32_t fontAtlasHeight,
                              uint32_t glyphWidth, uint32_t glyphHeight,
                              uint32_t columns)
{
    font_.Initialize(fontAtlasWidth, fontAtlasHeight,
                     glyphWidth, glyphHeight, columns);
}

void HudOverlay::BuildVertices(
    const GameFlowManager& flow,
    const RaceManager& race,
    const VehiclePhysics& physics,
    const CourseData& course,
    const RigidBody& vehicleBody,
    float fps,
    std::vector<SpriteVertex>& outVertices)
{
    GameState state = flow.GetCurrentState();

    switch (state)
    {
    case GameState::Countdown:
        BuildCountdown(race, race.GetTotalTime(), outVertices);
        BuildFPSCounter(fps, outVertices);
        break;

    case GameState::Racing:
        BuildRacingHud(race, physics, course, vehicleBody, outVertices);
        // Racing 開始直後 1 秒間は "GO!" を表示
        if (race.GetTotalTime() < 1.0f)
        {
            float scale = 6.0f;
            const char* goText = "GO!";
            float textW = font_.MeasureWidth(goText, scale);
            float textH = font_.GetLineHeight(scale);
            float cx = (kScreenWidth - textW) * 0.5f;
            float cy = (kScreenHeight - textH) * 0.5f;
            font_.GenerateTextVertices(goText, cx, cy, scale,
                                       0.0f, 1.0f, 0.0f, 1.0f, outVertices);
        }
        BuildFPSCounter(fps, outVertices);
        break;

    case GameState::Finished:
        BuildRacingHud(race, physics, course, vehicleBody, outVertices);
        {
            // "FINISH" バナーを画面中央に表示
            float scale = 5.0f;
            const char* finishText = "FINISH";
            float textW = font_.MeasureWidth(finishText, scale);
            float textH = font_.GetLineHeight(scale);
            float cx = (kScreenWidth - textW) * 0.5f;
            float cy = (kScreenHeight - textH) * 0.5f;
            font_.GenerateTextVertices(finishText, cx, cy, scale,
                                       1.0f, 0.8f, 0.0f, 1.0f, outVertices);
        }
        BuildFPSCounter(fps, outVertices);
        break;

    case GameState::Result:
        BuildResult(race, outVertices);
        break;

    default:
        break;
    }
}

void HudOverlay::BuildCountdown(const RaceManager& race,
                                  float /*totalTime*/,
                                  std::vector<SpriteVertex>& outVertices)
{
    int countdown = race.GetCountdownSeconds();
    if (countdown <= 0)
    {
        return;
    }

    // カウントダウン数字を画面中央に大きく表示
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%d", countdown);

    float scale = 6.0f;
    float textW = font_.MeasureWidth(buf, scale);
    float textH = font_.GetLineHeight(scale);
    float cx = (kScreenWidth - textW) * 0.5f;
    float cy = (kScreenHeight - textH) * 0.5f;

    font_.GenerateTextVertices(buf, cx, cy, scale,
                               1.0f, 1.0f, 0.0f, 1.0f, outVertices);
}

void HudOverlay::BuildRacingHud(const RaceManager& race,
                                  const VehiclePhysics& physics,
                                  const CourseData& course,
                                  const RigidBody& vehicleBody,
                                  std::vector<SpriteVertex>& outVertices)
{
    char buf[64];

    // --- ラップカウンター: 左上 ---
    std::snprintf(buf, sizeof(buf), "LAP %d/%d",
                  race.GetCurrentLap(), race.GetTotalLaps());
    font_.GenerateTextVertices(buf, 20.0f, 20.0f, 2.0f,
                               1.0f, 1.0f, 1.0f, 1.0f, outVertices);

    // --- 現在ラップタイム ---
    char timeBuf[16];
    FormatTime(race.GetCurrentLapTime(), timeBuf, sizeof(timeBuf));
    std::snprintf(buf, sizeof(buf), "TIME  %s", timeBuf);
    font_.GenerateTextVertices(buf, 20.0f, 56.0f, 1.5f,
                               1.0f, 1.0f, 1.0f, 1.0f, outVertices);

    // --- ベストラップタイム ---
    float bestTime = race.GetBestLapTime();
    if (bestTime > 0.0f)
    {
        FormatTime(bestTime, timeBuf, sizeof(timeBuf));
        std::snprintf(buf, sizeof(buf), "BEST  %s", timeBuf);
        font_.GenerateTextVertices(buf, 20.0f, 82.0f, 1.5f,
                                   0.3f, 1.0f, 0.3f, 1.0f, outVertices);
    }

    // --- 速度計: 右下 ---
    float speedKmh = std::abs(physics.GetSpeed()) * kMsToKmh;
    std::snprintf(buf, sizeof(buf), "%3d km/h", static_cast<int>(speedKmh));

    float speedScale = 3.0f;
    float speedW = font_.MeasureWidth(buf, speedScale);
    float speedX = kScreenWidth - speedW - 20.0f;
    float speedY = kScreenHeight - font_.GetLineHeight(speedScale) - 20.0f;
    font_.GenerateTextVertices(buf, speedX, speedY, speedScale,
                               1.0f, 1.0f, 1.0f, 1.0f, outVertices);

    // --- ミニマップ ---
    BuildMinimap(course, vehicleBody, outVertices);
}

void HudOverlay::BuildMinimap(const CourseData& course,
                                const RigidBody& vehicleBody,
                                std::vector<SpriteVertex>& outVertices)
{
    const CatmullRomSpline& spline = course.GetSpline();
    int segmentCount = spline.GetSegmentCount();
    if (segmentCount == 0)
    {
        return;
    }

    // 背景: 半透明黒矩形
    font_.GenerateQuadVertices(kMinimapX, kMinimapY, kMinimapSize, kMinimapSize,
                               0.0f, 0.0f, 0.0f, 0.5f, outVertices);

    // スプラインから等間隔に点をサンプリング
    float samplePoints[kMinimapSamples][2];  // XZ 座標
    float tStep = static_cast<float>(segmentCount) / static_cast<float>(kMinimapSamples);

    float minX =  1e9f, maxX = -1e9f;
    float minZ =  1e9f, maxZ = -1e9f;

    for (int i = 0; i < kMinimapSamples; ++i)
    {
        float t = static_cast<float>(i) * tStep;
        Vector3 pos = spline.Evaluate(t);
        samplePoints[i][0] = pos.x;
        samplePoints[i][1] = pos.z;

        minX = std::min(minX, pos.x);
        maxX = std::max(maxX, pos.x);
        minZ = std::min(minZ, pos.z);
        maxZ = std::max(maxZ, pos.z);
    }

    // AABB → ミニマップ矩形にマッピング (アスペクト比保持)
    float rangeX = maxX - minX;
    float rangeZ = maxZ - minZ;
    if (rangeX < 1.0f) rangeX = 1.0f;
    if (rangeZ < 1.0f) rangeZ = 1.0f;

    float mapScale;
    float offsetX, offsetZ;
    float padding = 10.0f;
    float usableSize = kMinimapSize - padding * 2.0f;

    if (rangeX > rangeZ)
    {
        mapScale = usableSize / rangeX;
        offsetX = kMinimapX + padding;
        offsetZ = kMinimapY + padding + (usableSize - rangeZ * mapScale) * 0.5f;
    }
    else
    {
        mapScale = usableSize / rangeZ;
        offsetX = kMinimapX + padding + (usableSize - rangeX * mapScale) * 0.5f;
        offsetZ = kMinimapY + padding;
    }

    // 隣接サンプル間を細い矩形で接続してコース輪郭を描画
    static constexpr float kLineWidth = 2.0f;

    for (int i = 0; i < kMinimapSamples; ++i)
    {
        int next = (i + 1) % kMinimapSamples;

        float ax = offsetX + (samplePoints[i][0] - minX) * mapScale;
        float ay = offsetZ + (samplePoints[i][1] - minZ) * mapScale;
        float bx = offsetX + (samplePoints[next][0] - minX) * mapScale;
        float by = offsetZ + (samplePoints[next][1] - minZ) * mapScale;

        // 線分の方向に垂直な法線で幅を持たせた矩形を生成
        float dx = bx - ax;
        float dy = by - ay;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) continue;

        float nx = -dy / len * kLineWidth * 0.5f;
        float ny =  dx / len * kLineWidth * 0.5f;

        // 4 頂点の矩形 → 6 頂点 (2 三角形)
        SpriteVertex verts[6];
        float u = (font_.MeasureWidth("A", 1.0f) > 0) ? 0.96875f : 0.96875f;
        float v = 0.9375f;

        auto fill = [&](SpriteVertex& sv, float px, float py)
        {
            sv.position[0] = px;
            sv.position[1] = py;
            sv.texCoord[0] = u;
            sv.texCoord[1] = v;
            sv.color[0] = 0.6f;
            sv.color[1] = 0.6f;
            sv.color[2] = 0.6f;
            sv.color[3] = 1.0f;
        };

        fill(verts[0], ax - nx, ay - ny);
        fill(verts[1], ax + nx, ay + ny);
        fill(verts[2], bx - nx, by - ny);
        fill(verts[3], bx - nx, by - ny);
        fill(verts[4], ax + nx, ay + ny);
        fill(verts[5], bx + nx, by + ny);

        outVertices.insert(outVertices.end(), verts, verts + 6);
    }

    // 車両位置を赤ドットとして表示
    static constexpr float kDotSize = 6.0f;
    float vehMapX = offsetX + (vehicleBody.position.x - minX) * mapScale;
    float vehMapZ = offsetZ + (vehicleBody.position.z - minZ) * mapScale;

    font_.GenerateQuadVertices(
        vehMapX - kDotSize * 0.5f, vehMapZ - kDotSize * 0.5f,
        kDotSize, kDotSize,
        1.0f, 0.2f, 0.2f, 1.0f, outVertices);
}

void HudOverlay::BuildResult(const RaceManager& race,
                               std::vector<SpriteVertex>& outVertices)
{
    // 半透明背景
    font_.GenerateQuadVertices(0.0f, 0.0f, kScreenWidth, kScreenHeight,
                               0.0f, 0.0f, 0.0f, 0.7f, outVertices);

    char buf[64];

    // タイトル
    float titleScale = 4.0f;
    const char* title = "RESULT";
    float titleW = font_.MeasureWidth(title, titleScale);
    float titleX = (kScreenWidth - titleW) * 0.5f;
    font_.GenerateTextVertices(title, titleX, 60.0f, titleScale,
                               1.0f, 0.8f, 0.0f, 1.0f, outVertices);

    // ラップタイム一覧
    float lapScale = 2.0f;
    float lapY = 180.0f;
    float lapLineHeight = font_.GetLineHeight(lapScale) + 8.0f;
    const auto& lapTimes = race.GetLapTimes();
    float bestLap = race.GetBestLapTime();

    for (std::size_t i = 0; i < lapTimes.size(); ++i)
    {
        char timeBuf[16];
        FormatTime(lapTimes[i], timeBuf, sizeof(timeBuf));
        std::snprintf(buf, sizeof(buf), "LAP %zu   %s", i + 1, timeBuf);

        // ベストラップは緑色で強調
        bool isBest = (bestLap > 0.0f) &&
                      (std::abs(lapTimes[i] - bestLap) < 0.001f);
        float r = isBest ? 0.3f : 1.0f;
        float g = 1.0f;
        float b = isBest ? 0.3f : 1.0f;

        float textW = font_.MeasureWidth(buf, lapScale);
        float textX = (kScreenWidth - textW) * 0.5f;
        font_.GenerateTextVertices(buf, textX, lapY, lapScale,
                                   r, g, b, 1.0f, outVertices);
        lapY += lapLineHeight;
    }

    // 合計タイム
    lapY += 16.0f;
    char totalTimeBuf[16];
    FormatTime(race.GetTotalTime(), totalTimeBuf, sizeof(totalTimeBuf));
    std::snprintf(buf, sizeof(buf), "TOTAL  %s", totalTimeBuf);
    float totalW = font_.MeasureWidth(buf, lapScale);
    float totalX = (kScreenWidth - totalW) * 0.5f;
    font_.GenerateTextVertices(buf, totalX, lapY, lapScale,
                               1.0f, 1.0f, 1.0f, 1.0f, outVertices);

    // リトライ案内
    lapY += lapLineHeight + 40.0f;
    const char* retryText = "Press R to Retry";
    float retryScale = 1.5f;
    float retryW = font_.MeasureWidth(retryText, retryScale);
    float retryX = (kScreenWidth - retryW) * 0.5f;
    font_.GenerateTextVertices(retryText, retryX, lapY, retryScale,
                               0.7f, 0.7f, 0.7f, 1.0f, outVertices);
}

void HudOverlay::BuildFPSCounter(float fps,
                                   std::vector<SpriteVertex>& outVertices)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "FPS:%3d", static_cast<int>(fps));

    // 左下隅に灰色で小さく表示 (ゲームプレイの邪魔にならないサイズ)
    float scale = 1.0f;
    float x = 20.0f;
    float y = kScreenHeight - font_.GetLineHeight(scale) - 10.0f;
    font_.GenerateTextVertices(buf, x, y, scale,
                               0.6f, 0.6f, 0.6f, 0.7f, outVertices);
}

void HudOverlay::FormatTime(float seconds, char* buffer, std::size_t bufferSize)
{
    if (seconds < 0.0f) seconds = 0.0f;

    int totalMs   = static_cast<int>(seconds * 1000.0f);
    int minutes   = totalMs / 60000;
    int secs      = (totalMs / 1000) % 60;
    int millis    = totalMs % 1000;

    std::snprintf(buffer, bufferSize, "%02d:%02d.%03d", minutes, secs, millis);
}

} // namespace Revora
