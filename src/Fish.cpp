// =============================================================================
// Fish.cpp  -  Fish AI implementation
// =============================================================================
#include "Fish.h"
#include <algorithm>

namespace nyx {

// --- species colour palettes (linear-ish; renderer applies fog/tonemap) ------
static Vec4 SpeciesColor(Species s, Rng& rng) {
    switch (s) {
        case Species::BlueTang:   return {0.10f, 0.35f, 0.95f, 1};
        case Species::YellowTang: return {1.00f, 0.78f, 0.05f, 1};
        case Species::Clownfish:  return {1.00f, 0.45f, 0.08f, 1};
        case Species::NeonTetra:  return {0.55f + rng.Range(-.05f,.05f), 0.75f, 0.85f, 1};
        case Species::Anthias:    return {1.00f, 0.30f, 0.65f, 1};
        case Species::Malware:    return {1.00f, 0.10f, 0.12f, 1};
        case Species::Shark:      return {0.32f, 0.36f, 0.42f, 1};
        default:                  return {0.7f, 0.7f, 0.7f, 1};
    }
}

void FishSystem::Init(unsigned seed, int count, float aspect, bool threatCreatures) {
    rng_ = Rng(seed);
    aspect_ = aspect;
    targetCount_ = count;
    threatCreatures_ = threatCreatures;

    fish_.clear();
    fish_.reserve(count + 4);
    for (int i = 0; i < count; ++i) {
        Fish f;
        SpawnFish(f, /*fromEdge*/ false); // initial cast is already in the tank
        f.alpha = 1.0f;                    // visible from the start
        fish_.push_back(f);
    }
    instances_.reserve(count + 8);
    food_.reserve(48);
}

void FishSystem::Resize(float aspect) { aspect_ = aspect; }

// -----------------------------------------------------------------------------
// Spawning. Most fish are common reef species; a handful are schooling shoals.
// Personalities are randomized so no two agents behave identically.
// -----------------------------------------------------------------------------
void FishSystem::SpawnFish(Fish& f, bool fromEdge) {
    // Weighted species selection — lots of small schoolers, fewer big showpieces.
    float r = rng_.Next01();
    if      (r < 0.45f) f.species = Species::NeonTetra;
    else if (r < 0.62f) f.species = Species::Anthias;
    else if (r < 0.76f) f.species = Species::YellowTang;
    else if (r < 0.88f) f.species = Species::BlueTang;
    else                f.species = Species::Clownfish;

    f.baseColor = SpeciesColor(f.species, rng_);

    // Size by species, with individual variation.
    float baseSize = 0.05f;
    switch (f.species) {
        case Species::BlueTang:   baseSize = 0.11f; break;
        case Species::YellowTang: baseSize = 0.10f; break;
        case Species::Clownfish:  baseSize = 0.075f; break;
        case Species::Anthias:    baseSize = 0.055f; break;
        case Species::NeonTetra:  baseSize = 0.045f; break;
        default: break;
    }
    f.scale = baseSize * rng_.Range(0.82f, 1.20f);

    // Personality vector.
    f.hunger    = rng_.Next01();
    f.curiosity = rng_.Next01();
    f.energy    = rng_.Range(0.3f, 1.0f);
    f.fearTrait = rng_.Range(0.2f, 1.0f);
    f.prefDepth = rng_.Range(0.15f, 0.9f);
    f.prefSpeed = rng_.Range(0.25f, 0.7f) * (0.6f + 0.6f * f.energy);
    f.schooling = (f.species == Species::NeonTetra) ? rng_.Range(0.7f, 1.0f)
                                                    : rng_.Range(0.1f, 0.7f);
    f.moodSeed  = rng_.Range(0, TWO_PI);
    f.maxSpeed  = f.prefSpeed * 1.6f;
    f.maxForce  = 1.6f + f.energy * 1.6f;
    f.lifeTimer = rng_.Range(40.0f, 140.0f);
    f.leaving   = false;
    f.fear      = 0;

    // Position. New arrivals slide in from a random edge at their preferred depth.
    f.pos.z = f.prefDepth;
    if (fromEdge) {
        bool left = rng_.Next01() < 0.5f;
        f.pos.x = left ? -aspect_ - 0.3f : aspect_ + 0.3f;
        f.pos.y = rng_.Range(-0.20f, 0.85f);   // water column, above the reef
        float dir = left ? 0.0f : PI;
        f.heading = dir + rng_.Range(-0.3f, 0.3f);
        f.alpha = 0.0f; // fade in
    } else {
        f.pos.x = rng_.Range(-aspect_ * 0.9f, aspect_ * 0.9f);
        f.pos.y = rng_.Range(-0.15f, 0.85f);   // water column, above the reef
        f.heading = rng_.Range(0, TWO_PI);
    }
    f.prevHeading = f.heading;
    f.vel = Vec3{std::cos(f.heading), std::sin(f.heading), 0} * (f.prefSpeed * 0.5f);
    f.phase = rng_.Range(0, TWO_PI);
}

void FishSystem::AddFood(const Vec3& p) {
    for (auto& fd : food_) if (!fd.active) { fd.pos = p; fd.life = 8.0f; fd.active = true; return; }
    if (food_.size() < 48) food_.push_back({p, 8.0f, true});
}

// -----------------------------------------------------------------------------
// Spatial hash — bucket fish into a uniform grid for neighbour queries.
// -----------------------------------------------------------------------------
void FishSystem::BuildSpatialHash() {
    cellSize_ = 0.30f;
    gridW_ = std::max(1, (int)std::ceil((aspect_ * 2.0f) / cellSize_) + 2);
    gridH_ = std::max(1, (int)std::ceil(2.0f / cellSize_) + 2);
    int cells = gridW_ * gridH_;

    std::vector<int> counts(cells, 0);
    auto cellOf = [&](const Vec3& p) {
        int cx = (int)((p.x + aspect_) / cellSize_); cx = std::clamp(cx, 0, gridW_ - 1);
        int cy = (int)((p.y + 1.0f)   / cellSize_); cy = std::clamp(cy, 0, gridH_ - 1);
        return cy * gridW_ + cx;
    };

    for (auto& f : fish_) counts[cellOf(f.pos)]++;

    hashCells_.assign(cells, {0, 0});
    int running = 0;
    for (int c = 0; c < cells; ++c) { hashCells_[c].start = running; hashCells_[c].count = counts[c]; running += counts[c]; }

    hashIndices_.assign(fish_.size(), 0);
    std::vector<int> cursor(cells, 0);
    for (int i = 0; i < (int)fish_.size(); ++i) {
        int c = cellOf(fish_[i].pos);
        hashIndices_[hashCells_[c].start + cursor[c]++] = i;
    }
}

void FishSystem::QueryNeighbors(const Vec3& p, float radius, std::vector<int>& out) const {
    out.clear();
    int r = std::max(1, (int)std::ceil(radius / cellSize_));
    int cx = std::clamp((int)((p.x + aspect_) / cellSize_), 0, gridW_ - 1);
    int cy = std::clamp((int)((p.y + 1.0f)   / cellSize_), 0, gridH_ - 1);
    for (int yy = cy - r; yy <= cy + r; ++yy) {
        if (yy < 0 || yy >= gridH_) continue;
        for (int xx = cx - r; xx <= cx + r; ++xx) {
            if (xx < 0 || xx >= gridW_) continue;
            const Cell& cell = hashCells_[yy * gridW_ + xx];
            for (int k = 0; k < cell.count; ++k) out.push_back(hashIndices_[cell.start + k]);
        }
    }
}

// -----------------------------------------------------------------------------
// Wander — smooth, non-repeating drift using layered noise on the heading.
// -----------------------------------------------------------------------------
Vec3 FishSystem::Wander(Fish& f, double time) const {
    float n = DriftNoise((float)time * 0.5f, f.moodSeed);
    float ang = f.heading + n * 0.9f;
    Vec3 dir{std::cos(ang), std::sin(ang), 0};
    // gentle vertical curiosity
    dir.y += DriftNoise((float)time * 0.31f, f.moodSeed * 1.7f) * 0.4f;
    return Normalize(dir) * f.maxSpeed;
}

// -----------------------------------------------------------------------------
// Core steering: blend boids rules + personality drives + threat avoidance.
// -----------------------------------------------------------------------------
void FishSystem::ComputeSteering(Fish& f, int idx, float dt, double time) {
    static thread_local std::vector<int> nb;
    float perception = f.scale * 6.0f + 0.25f;
    QueryNeighbors(f.pos, perception, nb);

    Vec3 sep{}, ali{}, coh{}; int sepN = 0, aliN = 0, cohN = 0;
    Vec3 fleeShark{}; float nearestPred = 1e9f;

    for (int j : nb) {
        if (j == idx) continue;
        Fish& o = fish_[j];
        Vec3 d = f.pos - o.pos;
        float dist = Length(d);
        if (dist < 1e-4f) continue;

        // Predators / malware: everyone but the predator flees.
        bool oIsPred = (o.species == Species::Shark || o.species == Species::Malware);
        if (oIsPred && f.species != Species::Shark && f.species != Species::Malware) {
            float scareR = (o.species == Species::Shark) ? 0.9f : 0.45f;
            if (dist < scareR) {
                fleeShark += Normalize(d) * (scareR - dist) / scareR;
                nearestPred = std::min(nearestPred, dist);
            }
            continue;
        }

        // Separation: avoid crowding (all neighbours).
        if (dist < perception * 0.5f) { sep += Normalize(d) * (1.0f / dist); sepN++; }

        // Alignment + cohesion only with same-ish species (a school).
        bool sameSchool = (o.species == f.species);
        if (sameSchool && dist < perception) {
            ali += o.vel; aliN++;
            coh += o.pos; cohN++;
        }
    }

    Vec3 steer{};
    // Separation is always important (collision avoidance).
    if (sepN) steer += Truncate(Normalize(sep) * f.maxSpeed - f.vel, f.maxForce) * 1.7f;

    // Alignment + cohesion scaled by this fish's schooling tendency.
    if (aliN) {
        Vec3 desired = Normalize(ali * (1.0f / aliN)) * f.maxSpeed;
        steer += Truncate(desired - f.vel, f.maxForce) * (1.0f * f.schooling);
    }
    if (cohN) {
        Vec3 center = coh * (1.0f / cohN);
        Vec3 desired = Normalize(center - f.pos) * f.maxSpeed;
        steer += Truncate(desired - f.vel, f.maxForce) * (0.9f * f.schooling);
    }

    // Flee predators — overrides everything when close.
    if (LengthSq(fleeShark) > 1e-5f) {
        f.fear = std::min(1.0f, f.fear + (1.0f - nearestPred) * f.fearTrait * dt * 6.0f);
        Vec3 desired = Normalize(fleeShark) * f.maxSpeed * 1.8f;
        steer += Truncate(desired - f.vel, f.maxForce * 3.0f) * (2.5f * (0.5f + f.fearTrait));
    }
    f.fear = std::max(0.0f, f.fear - dt * 0.4f);

    // Food seeking (hunger drive).
    if (!food_.empty()) {
        float best = 1e9f; const Food* target = nullptr;
        for (auto& fd : food_) {
            if (!fd.active) continue;
            float d = LengthSq(fd.pos - f.pos);
            if (d < best) { best = d; target = &fd; }
        }
        if (target && best < 1.5f * 1.5f) {
            Vec3 desired = Normalize(target->pos - f.pos) * f.maxSpeed;
            steer += Truncate(desired - f.vel, f.maxForce) * (0.6f + f.hunger * 1.4f);
        }
    }

    // Preferred-depth drive: gently pull toward the fish's comfort layer.
    float depthErr = f.prefDepth - f.pos.z;
    f.pos.z += depthErr * std::min(1.0f, dt * 0.6f); // depth is quasi-1D, integrate directly

    // Wander baseline (curiosity).
    steer += Truncate(Wander(f, time) - f.vel, f.maxForce) * (0.4f + f.curiosity * 0.5f);

    // Soft boundary avoidance. Side-on tank: fish roam the full width but stay
    // between the surface (top) and the reef crest (bottom) so the scene reads
    // as a water column above the reef rather than a top-down map.
    if (!f.leaving) {
        const float mx = 0.35f;
        const float surfaceY = 0.90f;   // just below the water surface
        const float reefY    = -0.46f;  // just above the coral crest
        if (f.pos.x >  aspect_ - mx) steer.x -= (f.pos.x - (aspect_ - mx)) * 8.0f;
        if (f.pos.x < -aspect_ + mx) steer.x += ((-aspect_ + mx) - f.pos.x) * 8.0f;
        if (f.pos.y >  surfaceY)     steer.y -= (f.pos.y - surfaceY) * 10.0f;
        if (f.pos.y <  reefY)        steer.y += (reefY - f.pos.y) * 12.0f;  // don't dive into coral
    }

    // Integrate velocity with momentum & clamp.
    f.vel += Truncate(steer, f.maxForce * 2.0f) * dt;
    float spd = Length(f.vel);
    float maxS = f.maxSpeed * (1.0f + f.fear * 1.2f);
    if (spd > maxS) f.vel = f.vel * (maxS / spd);
    if (spd < 0.02f) f.vel += Vec3{std::cos(f.heading), std::sin(f.heading), 0} * 0.05f;
}

// -----------------------------------------------------------------------------
// Threat creature events (cyber theme). Rare, brief, and educational rather
// than alarming. Higher machine load nudges probability up slightly.
// -----------------------------------------------------------------------------
void FishSystem::MaybeSpawnThreat(float dt, float loadFactor) {
    if (!threatCreatures_) return;

    threatCooldown_ -= dt;
    if (sharkActive_) {
        sharkTimer_ -= dt;
        if (sharkTimer_ <= 0) {
            // Mark predators leaving.
            for (auto& f : fish_) if (f.species == Species::Shark || f.species == Species::Malware) f.leaving = true;
            sharkActive_ = false;
            threatCooldown_ = rng_.Range(30.0f, 70.0f);
        }
        return;
    }

    if (threatCooldown_ > 0) return;
    // Probability scales gently with system load.
    float chance = (0.004f + loadFactor * 0.01f);
    if (rng_.Next01() > chance) return;

    // Spawn either a lone malware fish or a ransomware shark.
    bool shark = rng_.Next01() < 0.5f;
    Fish p;
    SpawnFish(p, true);
    p.species = shark ? Species::Shark : Species::Malware;
    p.baseColor = SpeciesColor(p.species, rng_);
    p.scale = shark ? 0.34f : 0.07f;
    p.maxSpeed = shark ? 0.55f : 0.8f;
    p.maxForce = shark ? 2.0f : 3.0f;
    p.prefDepth = rng_.Range(0.3f, 0.7f);
    p.schooling = 0;
    p.alpha = 0;
    fish_.push_back(p);

    sharkActive_ = true;
    sharkTimer_ = shark ? rng_.Range(14.0f, 22.0f) : rng_.Range(10.0f, 16.0f);
}

// -----------------------------------------------------------------------------
// Per-frame update.
// -----------------------------------------------------------------------------
void FishSystem::Update(float dt, double time, float loadFactor) {
    dt = std::min(dt, 0.05f); // clamp to keep the sim stable on hitches

    // Maintain population: replace fish that have fully left, occasionally
    // cycle one out so the cast slowly changes (seamless, no fixed timer).
    for (size_t i = 0; i < fish_.size();) {
        Fish& f = fish_[i];
        bool predator = (f.species == Species::Shark || f.species == Species::Malware);
        bool gone = f.leaving && f.alpha <= 0.01f &&
                    (f.pos.x < -aspect_ - 0.2f || f.pos.x > aspect_ + 0.2f || predator);
        if (gone) {
            if (predator) { fish_.erase(fish_.begin() + i); continue; }
            SpawnFish(f, true); // respawn as a fresh arrival from the edge
        }
        ++i;
    }
    // Keep target population for non-predators.
    int nonPred = 0; for (auto& f : fish_) if (f.species != Species::Shark && f.species != Species::Malware) ++nonPred;
    while (nonPred < targetCount_) { Fish f; SpawnFish(f, true); fish_.push_back(f); ++nonPred; }

    MaybeSpawnThreat(dt, loadFactor);
    BuildSpatialHash();

    for (int i = 0; i < (int)fish_.size(); ++i) {
        Fish& f = fish_[i];

        // Occasionally decide to leave (life-cycle), then drift offscreen.
        f.lifeTimer -= dt;
        if (!f.leaving && f.lifeTimer <= 0 && f.species != Species::Shark && f.species != Species::Malware) {
            if (rng_.Next01() < 0.3f) f.leaving = true;
            else f.lifeTimer = rng_.Range(30.0f, 90.0f);
        }

        ComputeSteering(f, i, dt, time);

        // Integrate position (xy from velocity; z handled in steering).
        f.pos += f.vel * dt;

        // Heading & body bend from turning rate.
        if (LengthSq(f.vel) > 1e-5f) {
            float targetHeading = std::atan2(f.vel.y, f.vel.x);
            // shortest-arc damp
            float diff = targetHeading - f.heading;
            while (diff >  PI) diff -= TWO_PI;
            while (diff < -PI) diff += TWO_PI;
            float turn = diff * std::min(1.0f, dt * 8.0f);
            f.heading += turn;
            f.bend = Clamp(diff * 1.5f, -1.0f, 1.0f);
        }

        // Tail oscillation: faster beat with more speed / energy / fear.
        float spd = Length(f.vel);
        float beat = 4.0f + spd * 6.0f + f.fear * 6.0f;
        f.phase += beat * dt;
        if (f.phase > TWO_PI) f.phase -= TWO_PI;

        // Fade in / out.
        float targetAlpha = f.leaving ? 0.0f : 1.0f;
        f.alpha = Damp(f.alpha, targetAlpha, 1.5f, dt);
    }

    // Food lifetime.
    for (auto& fd : food_) if (fd.active) { fd.life -= dt; fd.pos.y -= dt * 0.05f; if (fd.life <= 0) fd.active = false; }

    RebuildInstances(dt);
}

void FishSystem::RebuildInstances(float dt) {
    instances_.clear();
    instances_.reserve(fish_.size());
    for (auto& f : fish_) {
        FishInstance fi{};
        fi.pos = f.pos;
        fi.scale = f.scale;
        fi.heading = f.heading;
        fi.phase = f.phase;
        fi.bend = f.bend;
        float spd = Length(f.vel);
        fi.beat = 1.0f + spd * 2.0f + f.fear;
        fi.species = (float)(int)f.species;
        fi.tint = f.baseColor;
        fi.tint.w = f.alpha;
        fi.glow = (f.species == Species::Malware) ? 0.8f + 0.2f * std::sin(f.phase * 3.0f)
                : (f.species == Species::Shark)   ? 0.15f : 0.0f;
        instances_.push_back(fi);
    }
    // Sort far -> near for correct alpha blending.
    std::sort(instances_.begin(), instances_.end(),
              [](const FishInstance& a, const FishInstance& b) { return a.pos.z > b.pos.z; });
}

} // namespace nyx
