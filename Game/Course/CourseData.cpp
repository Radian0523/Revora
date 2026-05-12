#include "CourseData.h"

#include <nlohmann/json.hpp>
#include <SDL.h>

#include <fstream>

namespace Revora {

bool CourseData::LoadFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Course data not found: %s (using defaults)", filepath.c_str());
        return true;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to parse course data: %s", e.what());
        return false;
    }

    // 制御点の読み込み: [[x,y,z], [x,y,z], ...] 形式
    if (j.contains("controlPoints") && j["controlPoints"].is_array()) {
        controlPoints.clear();
        for (const auto& pt : j["controlPoints"]) {
            if (pt.is_array() && pt.size() == 3) {
                controlPoints.emplace_back(
                    pt[0].get<float>(),
                    pt[1].get<float>(),
                    pt[2].get<float>()
                );
            }
        }
    }

    if (j.contains("trackWidth")) {
        trackWidth = j["trackWidth"].get<float>();
    }
    if (j.contains("lapCount")) {
        lapCount = j["lapCount"].get<int>();
    }

    // チェックポイント位置の読み込み
    if (j.contains("checkpoints") && j["checkpoints"].is_array()) {
        checkpointPositions.clear();
        for (const auto& cp : j["checkpoints"]) {
            checkpointPositions.push_back(cp.get<float>());
        }
    }

    if (j.contains("spawnT")) {
        spawnT = j["spawnT"].get<float>();
    }

    SDL_Log("Course data loaded: %s (%zu control points, %zu checkpoints)",
            filepath.c_str(), controlPoints.size(), checkpointPositions.size());
    return true;
}

void CourseData::BuildSpline()
{
    if (controlPoints.size() < 3) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Course needs at least 3 control points (got %zu)",
                     controlPoints.size());
        return;
    }

    spline_.Build(controlPoints);

    SDL_Log("Course spline built: %d segments, total length: %.1f m",
            spline_.GetSegmentCount(), spline_.GetTotalLength());
}

} // namespace Revora
