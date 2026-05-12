#pragma once

#include "../Math/Vector3.h"

#include <string>

namespace Revora {

/// 車両パラメータ構造体
/// JSON ファイルで外部から調整可能なデータドリブン設計
/// 新しい車両の追加は JSON ファイルを増やすだけで完了する
struct VehicleConfig {
    // --- 車体 ---
    float mass             = 1200.0f;    // kg
    float dragCoefficient  = 0.4f;
    float rollingResistance = 8.0f;

    // --- エンジン / ブレーキ ---
    float engineTorque     = 4000.0f;    // N
    float brakeTorque      = 6000.0f;    // N
    float maxSpeed         = 40.0f;      // m/s

    // --- ステアリング ---
    float maxSteerAngle    = 35.0f;      // 度 (実行時にラジアンに変換)
    float steerSpeed       = 4.0f;       // ステアリングの反応速度 (rad/s 相当)

    // --- サスペンション ---
    float suspensionRestLength = 0.35f;  // m
    float suspensionStiffness  = 30000.0f;  // N/m
    float suspensionDamping    = 3000.0f;   // Ns/m
    float wheelRadius          = 0.3f;      // m

    // --- タイヤ ---
    float tireGripFactor       = 1.5f;
    float slipAngleThreshold   = 8.0f;   // 度 (Pacejka カーブのピーク位置)

    // --- ホイール配置 (車体ローカル座標) ---
    // 車体中心からの相対オフセット
    float wheelbaseFront = 1.2f;   // 前輪の前方オフセット
    float wheelbaseRear  = 1.2f;   // 後輪の後方オフセット
    float trackWidth     = 0.8f;   // 左右の間隔 (片側)
    float wheelHeight    = 0.0f;   // サスペンション取り付け高さ

    // --- リセット ---
    Vector3 spawnPosition  = {0.0f, 0.5f, 0.0f};
    float   spawnYaw       = 0.0f;  // ラジアン

    /// JSON ファイルから車両パラメータを読み込む
    /// ファイルが存在しない場合はデフォルト値を使用し true を返す
    bool LoadFromFile(const std::string& filepath);
};

} // namespace Revora
