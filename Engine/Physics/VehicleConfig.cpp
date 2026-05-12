#include "VehicleConfig.h"

#include <nlohmann/json.hpp>
#include <SDL.h>

#include <fstream>

namespace Revora {

/// JSON キーが存在する場合のみ値を上書きするヘルパー
/// パラメータの部分指定を可能にし、JSON に書かれていない値はデフォルトのままにする
template <typename T>
static void ReadIfExists(const nlohmann::json& j, const char* key, T& out)
{
    if (j.contains(key)) {
        out = j[key].get<T>();
    }
}

bool VehicleConfig::LoadFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Vehicle config not found: %s (using defaults)", filepath.c_str());
        return true;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(file);
    }
    catch (const nlohmann::json::parse_error& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to parse vehicle config: %s", e.what());
        return false;
    }

    // --- 車体 ---
    ReadIfExists(j, "mass",              mass);
    ReadIfExists(j, "dragCoefficient",   dragCoefficient);
    ReadIfExists(j, "rollingResistance", rollingResistance);

    // --- エンジン / ブレーキ ---
    ReadIfExists(j, "engineTorque", engineTorque);
    ReadIfExists(j, "brakeTorque",  brakeTorque);
    ReadIfExists(j, "maxSpeed",     maxSpeed);

    // --- ステアリング ---
    ReadIfExists(j, "maxSteerAngle", maxSteerAngle);
    ReadIfExists(j, "steerSpeed",    steerSpeed);

    // --- サスペンション ---
    ReadIfExists(j, "suspensionRestLength", suspensionRestLength);
    ReadIfExists(j, "suspensionStiffness",  suspensionStiffness);
    ReadIfExists(j, "suspensionDamping",    suspensionDamping);
    ReadIfExists(j, "wheelRadius",          wheelRadius);

    // --- タイヤ ---
    ReadIfExists(j, "tireGripFactor",     tireGripFactor);
    ReadIfExists(j, "slipAngleThreshold", slipAngleThreshold);

    // --- ホイール配置 ---
    ReadIfExists(j, "wheelbaseFront", wheelbaseFront);
    ReadIfExists(j, "wheelbaseRear",  wheelbaseRear);
    ReadIfExists(j, "trackWidth",     trackWidth);
    ReadIfExists(j, "wheelHeight",    wheelHeight);

    // --- スポーン ---
    if (j.contains("spawnPosition") && j["spawnPosition"].is_array()
        && j["spawnPosition"].size() == 3) {
        spawnPosition.x = j["spawnPosition"][0].get<float>();
        spawnPosition.y = j["spawnPosition"][1].get<float>();
        spawnPosition.z = j["spawnPosition"][2].get<float>();
    }
    ReadIfExists(j, "spawnYaw", spawnYaw);

    SDL_Log("Vehicle config loaded: %s", filepath.c_str());
    return true;
}

} // namespace Revora
