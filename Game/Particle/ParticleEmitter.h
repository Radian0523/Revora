#pragma once

#include "../../Engine/Math/Vector3.h"

#include <vector>
#include <cstdint>

namespace Revora {

/// 個々のパーティクルの状態
struct Particle {
    Vector3 position;
    Vector3 velocity;
    float   lifetime = 0.0f;  // 最大生存時間 (秒)
    float   age      = 0.0f;  // 経過時間 (秒)
    float   size     = 0.5f;
    float   alpha    = 1.0f;
    Vector3 color    = {1.0f, 1.0f, 1.0f};
};

/// パーティクル生成時のパラメータ設定
struct ParticleConfig {
    float minLifetime = 0.3f;
    float maxLifetime = 0.8f;
    float minSize     = 0.2f;
    float maxSize     = 0.5f;
    float minSpeed    = 1.0f;
    float maxSpeed    = 3.0f;
    float initialAlpha = 1.0f;
    Vector3 color     = {1.0f, 1.0f, 1.0f};

    // サイズの経時変化: 正で膨張、負で収縮
    float sizeGrowthRate = 0.0f;
};

/// パーティクルの生成・更新・消滅を管理する
/// オブジェクトプールパターンで固定サイズの配列を再利用し、
/// 頻繁な生成・破棄によるメモリアロケーションを回避する
class ParticleEmitter {
public:
    void Initialize(int maxParticles);

    /// 指定位置からパーティクルを発生させる
    /// direction は射出方向 (正規化済み)、スプレッド角で散布される
    void Emit(const Vector3& position, const Vector3& direction,
              int count, const ParticleConfig& config);

    /// パーティクルの物理更新と寿命管理
    void Update(float dt);

    /// アクティブなパーティクルの取得 (ParticleRenderer に渡す)
    const std::vector<Particle>& GetActiveParticles() const { return particles_; }
    uint32_t GetActiveCount() const { return activeCount_; }

    /// 全パーティクルをクリアする
    void Reset();

private:
    /// 乱数生成 (シンプルな線形合同法)
    float RandomFloat(float min, float max);

    std::vector<Particle> particles_;
    uint32_t activeCount_ = 0;
    uint32_t maxParticles_ = 0;
    uint32_t randomSeed_   = 12345;

    // 重力加速度 (パーティクルに適用する簡易的な重力)
    static constexpr float kParticleGravity = 5.0f;
};

} // namespace Revora
