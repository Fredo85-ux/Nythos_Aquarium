// =============================================================================
// ParticleSystem.cpp
// =============================================================================
#include "ParticleSystem.h"
#include <algorithm>

namespace nyx {

void ParticleSystem::Init(unsigned seed, float aspect, float density) {
    rng_ = Rng(seed);
    aspect_ = aspect;
    density_ = density;
    pool_.assign(4096, {});
    instances_.reserve(4096);

    // Scatter a handful of bubble vents along the reef floor.
    vents_.clear();
    int n = 7;
    for (int i = 0; i < n; ++i)
        vents_.push_back({rng_.Range(-aspect_ * 0.9f, aspect_ * 0.9f),
                          rng_.Range(-0.98f, -0.85f),
                          rng_.Range(0.2f, 0.85f)});

    // Pre-seed some drifting motes so the tank isn't empty on the first frame.
    for (int i = 0; i < 300; ++i) Emit(1);
}

void ParticleSystem::Emit(int kind) {
    for (auto& p : pool_) {
        if (p.active) continue;
        p.active = true;
        p.kind = kind;
        p.seed = rng_.Range(0, TWO_PI);
        if (kind == 0) { // bubble from a random vent
            const Vec3& v = vents_[rng_.RangeI(0, (int)vents_.size() - 1)];
            p.pos = {v.x + rng_.Range(-0.05f, 0.05f), v.y, v.z};
            p.vel = {0, rng_.Range(0.18f, 0.4f), 0};
            p.size = rng_.Range(0.004f, 0.018f);
            p.maxLife = rng_.Range(4.0f, 8.0f);
        } else {         // suspended mote anywhere in the column
            p.pos = {rng_.Range(-aspect_, aspect_), rng_.Range(-1, 1), rng_.Range(0.05f, 0.95f)};
            p.vel = {rng_.Range(-0.02f, 0.02f), rng_.Range(-0.01f, 0.02f), 0};
            p.size = rng_.Range(0.0015f, 0.005f);
            p.maxLife = rng_.Range(8.0f, 20.0f);
        }
        p.life = p.maxLife;
        return;
    }
}

void ParticleSystem::Update(float dt, double time) {
    dt = std::min(dt, 0.05f);

    // Stochastic emission. Rates scale with the density setting.
    bubbleAccum_ += dt * 22.0f * density_;
    moteAccum_   += dt * 16.0f * density_;
    while (bubbleAccum_ >= 1.0f) { Emit(0); bubbleAccum_ -= 1.0f; }
    while (moteAccum_   >= 1.0f) { Emit(1); moteAccum_   -= 1.0f; }

    instances_.clear();
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0) { p.active = false; continue; }

        if (p.kind == 0) {
            // Bubbles accelerate slightly and wobble sinusoidally as they rise.
            p.vel.y += dt * 0.05f;
            p.pos.y += p.vel.y * dt;
            p.pos.x += std::sin((float)time * 2.0f + p.seed) * 0.06f * dt;
            if (p.pos.y > 1.02f) { p.active = false; continue; }
        } else {
            // Motes drift with a slow turbulent field.
            p.pos.x += (p.vel.x + std::sin((float)time * 0.3f + p.seed) * 0.01f) * dt;
            p.pos.y += (p.vel.y + std::cos((float)time * 0.21f + p.seed * 1.7f) * 0.008f) * dt;
        }

        float lifeFrac = p.life / p.maxLife;
        float alpha = (p.kind == 0) ? std::min(1.0f, lifeFrac * 4.0f) * 0.5f
                                    : std::min(1.0f, lifeFrac * 2.0f) * 0.35f;

        ParticleInstance pi{};
        pi.pos = p.pos;
        pi.size = p.size;
        pi.kind = (float)p.kind;
        pi.color = (p.kind == 0) ? Vec4{0.8f, 0.95f, 1.0f, alpha}
                                 : Vec4{0.6f, 0.8f, 0.9f, alpha};
        instances_.push_back(pi);
    }

    // Depth sort for clean blending.
    std::sort(instances_.begin(), instances_.end(),
              [](const ParticleInstance& a, const ParticleInstance& b) { return a.pos.z > b.pos.z; });
}

} // namespace nyx
