#include "ParticleEmitter.h"

#include <cmath>

namespace Revora {

void ParticleEmitter::Initialize(int maxParticles)
{
    pool_.Initialize(static_cast<uint32_t>(maxParticles));
}

void ParticleEmitter::Emit(const Vector3& position, const Vector3& direction,
                            int count, const ParticleConfig& config)
{
    for (int i = 0; i < count; ++i) {
        Particle* p = pool_.Acquire();
        if (p == nullptr) {
            break;
        }

        p->position = position;
        p->lifetime = RandomFloat(config.minLifetime, config.maxLifetime);
        p->age      = 0.0f;
        p->size     = RandomFloat(config.minSize, config.maxSize);
        p->alpha    = config.initialAlpha;
        p->color    = config.color;

        // 射出方向にランダムな散布を加える
        float speed = RandomFloat(config.minSpeed, config.maxSpeed);
        float spreadX = RandomFloat(-0.3f, 0.3f);
        float spreadY = RandomFloat(0.0f, 0.5f);
        float spreadZ = RandomFloat(-0.3f, 0.3f);

        p->velocity = (direction + Vector3(spreadX, spreadY, spreadZ)).Normalized() * speed;
    }
}

void ParticleEmitter::Update(float dt)
{
    uint32_t i = 0;
    while (i < pool_.GetActiveCount()) {
        Particle& p = pool_[i];
        p.age += dt;

        if (p.age >= p.lifetime) {
            // 寿命切れ: swap-and-pop で O(1) 除去
            pool_.Release(i);
            continue;
        }

        // 正規化された寿命進行率 (0.0 = 生成直後, 1.0 = 寿命切れ)
        float t = p.age / p.lifetime;

        // 位置更新
        p.velocity.y -= kParticleGravity * dt;
        p.position += p.velocity * dt;

        // フェードアウト: 寿命の後半でアルファが減少
        p.alpha = 1.0f - t;

        // サイズの経時変化 (スモーク: 膨張しながらフェードアウト)
        p.size += p.size * 0.5f * dt;

        ++i;
    }
}

void ParticleEmitter::Reset()
{
    pool_.Reset();
}

float ParticleEmitter::RandomFloat(float min, float max)
{
    // 線形合同法: 外部ライブラリ不要、再現性のある擬似乱数
    randomSeed_ = randomSeed_ * 1103515245u + 12345u;
    float normalized = static_cast<float>(randomSeed_ & 0x7FFFFFFFu) / 2147483647.0f;
    return min + normalized * (max - min);
}

} // namespace Revora
