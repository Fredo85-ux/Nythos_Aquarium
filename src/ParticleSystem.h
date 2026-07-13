// =============================================================================
// ParticleSystem.h  -  Bubbles and floating motes
// -----------------------------------------------------------------------------
// Two emitter classes share one instanced point-sprite draw:
//   * Bubbles    - rise from randomized reef vents, wobble, pop at the surface.
//   * Motes/dust - tiny suspended particles drifting in the current.
// Emission is stochastic (Poisson-ish), so the field never visibly repeats.
// =============================================================================
#pragma once
#include "NyxMath.h"
#include <vector>

namespace nyx {

struct ParticleInstance {
    Vec3  pos;
    float size;
    Vec4  color;   // a = alpha
    float kind;    // 0 = bubble, 1 = mote
    float pad0, pad1, pad2;
};
static_assert(sizeof(ParticleInstance) % 16 == 0, "ParticleInstance must be 16-byte aligned");

class ParticleSystem {
public:
    void Init(unsigned seed, float aspect, float density);
    void Resize(float aspect) { aspect_ = aspect; }
    void SetDensity(float d)   { density_ = d; }
    void Update(float dt, double time);
    const std::vector<ParticleInstance>& Instances() const { return instances_; }

private:
    struct P {
        Vec3 pos, vel; float life, maxLife, size, seed; int kind; bool active;
    };
    void Emit(int kind);

    std::vector<P> pool_;
    std::vector<ParticleInstance> instances_;
    std::vector<Vec3> vents_;        // fixed bubble-emitter mouths on the reef
    Rng   rng_{0x51EEDu};
    float aspect_ = 1.777f, density_ = 1.0f;
    float bubbleAccum_ = 0, moteAccum_ = 0;
};

} // namespace nyx
