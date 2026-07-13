// =============================================================================
// Fish.h  -  Autonomous fish agents (Fish AI module)
// -----------------------------------------------------------------------------
// Each fish is an independent agent, not a looping animation. Behaviour emerges
// from steering forces (Reynolds boids: separation / cohesion / alignment) plus
// a personality vector (hunger, curiosity, energy, fear, preferred depth/speed,
// schooling tendency) and a slowly-drifting mood. Threat creatures (malware
// fish, ransomware shark) inject fear that makes schools scatter.
//
// The simulation runs in a normalized world box:
//     x in [-aspect, +aspect],  y in [-1, +1],  z (depth) in [0, 1]
// where z = 0 is closest to the glass and z = 1 is far into the murk. Depth
// drives parallax scale and fog tint in the renderer.
//
// Output: a tightly packed array of FishInstance structs uploaded once per
// frame to a single instanced draw call (one call for the whole school).
// =============================================================================
#pragma once
#include "NyxMath.h"
#include <vector>
#include <cstdint>

namespace nyx {

// GPU-facing per-instance data. Layout matches the HLSL instance buffer.
struct FishInstance {
    Vec3  pos;        // world position
    float scale;      // body length in world units
    Vec4  tint;       // rgb colour, a = fade alpha (enter/leave)
    float heading;    // travel direction in the xy-plane (radians)
    float phase;      // tail-oscillation phase
    float bend;       // body bend from turning (-1..1)
    float beat;       // tail-beat frequency scale (from speed/energy)
    float species;    // shape/colour family index
    float glow;       // emissive boost (cyber creatures pulse)
    float pad0, pad1;
};
static_assert(sizeof(FishInstance) % 16 == 0, "FishInstance must be 16-byte aligned");

enum class Species : int {
    BlueTang = 0,
    YellowTang,
    Clownfish,
    NeonTetra,    // schooling silver shoal
    Anthias,      // pink reef fish
    Malware,      // red "infected" agent (cyber event)
    Shark,        // ransomware predator (cyber event)
    COUNT
};

// Full simulation state for one agent (CPU side only).
struct Fish {
    Vec3  pos{}, vel{};
    float heading = 0, prevHeading = 0, bend = 0, phase = 0;
    float scale = 0.1f;
    Species species = Species::NeonTetra;

    // Personality (set once at spawn, never identical between fish).
    float hunger = 0, curiosity = 0, energy = 0, fearTrait = 0;
    float prefDepth = 0.5f, prefSpeed = 0.4f, schooling = 0.5f;

    // Transient mood / drives.
    float fear = 0;         // spikes near predators, decays over time
    float moodSeed = 0;     // phase offset for per-fish noise
    float maxSpeed = 0.6f, maxForce = 2.0f;

    // Life-cycle: fish drift in from offscreen and leave again so the tank
    // never feels like a fixed cast. alpha fades the body in/out.
    float alpha = 0;        // 0..1 visibility
    bool  leaving = false;
    float lifeTimer = 0;    // seconds until it considers leaving

    Vec4  baseColor{};
};

struct Food {
    Vec3 pos{}; float life = 0; bool active = false;
};

class FishSystem {
public:
    void Init(unsigned seed, int count, float aspect, bool threatCreatures);
    void Resize(float aspect);
    void SetThreatCreatures(bool on) { threatCreatures_ = on; }

    // Advance the simulation. `loadFactor` (0..1, from SystemMonitor) gently
    // raises threat-event probability so the tank mirrors machine activity.
    void Update(float dt, double time, float loadFactor);

    // Drop a bit of food at a world position (bubbles/curiosity also use this).
    void AddFood(const Vec3& p);

    // Packed instance data for rendering (sorted back-to-front for blending).
    const std::vector<FishInstance>& Instances() const { return instances_; }

    int  AliveCount() const { return (int)fish_.size(); }
    bool SharkActive() const { return sharkActive_; }

private:
    Vec3 Wander(Fish& f, double time) const;
    void ComputeSteering(Fish& f, int idx, float dt, double time);
    void SpawnFish(Fish& f, bool fromEdge);
    void MaybeSpawnThreat(float dt, float loadFactor);
    void BuildSpatialHash();
    void QueryNeighbors(const Vec3& p, float radius, std::vector<int>& out) const;
    void RebuildInstances(float dt);

    std::vector<Fish>         fish_;
    std::vector<FishInstance> instances_;
    std::vector<Food>         food_;

    // Uniform-grid spatial hash so boids stay O(n) instead of O(n^2).
    struct Cell { int start, count; };
    std::vector<int>  hashIndices_;   // fish indices sorted by cell
    std::vector<Cell> hashCells_;
    int   gridW_ = 1, gridH_ = 1;
    float cellSize_ = 0.3f;

    Rng   rng_{0xA9F1u};
    float aspect_ = 1.777f;
    int   targetCount_ = 140;
    bool  threatCreatures_ = true;

    bool  sharkActive_ = false;
    float sharkTimer_ = 0;
    float threatCooldown_ = 25.0f;
};

} // namespace nyx
