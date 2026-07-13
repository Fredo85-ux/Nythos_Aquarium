// =============================================================================
// Math.h  -  Lightweight vector math for the Nythos Cyber Aquarium
// -----------------------------------------------------------------------------
// A tiny, dependency-free math layer (we deliberately avoid pulling in a heavy
// library). Just enough 2D/3D vector algebra for steering behaviours, particle
// motion and shader constant packing. All types are POD so they map directly
// onto HLSL cbuffer layouts.
// =============================================================================
#pragma once
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace nyx {

constexpr float PI      = 3.14159265358979323846f;
constexpr float TWO_PI  = 6.28318530717958647692f;
constexpr float DEG2RAD = PI / 180.0f;

// -------------------------------------------------------------------------
// 2D vector
// -------------------------------------------------------------------------
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s)       const { return {x * s, y * s}; }
    Vec2 operator/(float s)       const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s)       { x *= s; y *= s; return *this; }
};

// -------------------------------------------------------------------------
// 3D vector
// -------------------------------------------------------------------------
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s)       const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s)       const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s; y *= s; z *= s; return *this; }
};

struct Vec4 { float x = 0, y = 0, z = 0, w = 0; };

// -------------------------------------------------------------------------
// Free functions
// -------------------------------------------------------------------------
inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline float LengthSq(const Vec2& v) { return v.x * v.x + v.y * v.y; }
inline float LengthSq(const Vec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline float Length(const Vec2& v)   { return std::sqrt(LengthSq(v)); }
inline float Length(const Vec3& v)   { return std::sqrt(LengthSq(v)); }

inline Vec2 Normalize(const Vec2& v) {
    float len = Length(v);
    return len > 1e-6f ? v / len : Vec2{0, 0};
}
inline Vec3 Normalize(const Vec3& v) {
    float len = Length(v);
    return len > 1e-6f ? v / len : Vec3{0, 0, 0};
}

// Clamp a vector's magnitude to [0, maxLen].
inline Vec2 Truncate(const Vec2& v, float maxLen) {
    float len = Length(v);
    if (len > maxLen && len > 1e-6f) return v * (maxLen / len);
    return v;
}
inline Vec3 Truncate(const Vec3& v, float maxLen) {
    float len = Length(v);
    if (len > maxLen && len > 1e-6f) return v * (maxLen / len);
    return v;
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Lerp(float a, float b, float t)    { return a + (b - a) * t; }
inline Vec3  Lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

// Frame-rate independent exponential smoothing toward a target.
inline float Damp(float current, float target, float rate, float dt) {
    return Lerp(current, target, 1.0f - std::exp(-rate * dt));
}
inline Vec3 Damp(const Vec3& current, const Vec3& target, float rate, float dt) {
    return Lerp(current, target, 1.0f - std::exp(-rate * dt));
}

// -------------------------------------------------------------------------
// Random helpers (xorshift — fast, good enough for organic motion, and we
// never want the synchronized patterns a low-quality PRNG would create).
// -------------------------------------------------------------------------
class Rng {
public:
    explicit Rng(unsigned seed = 0x1337C0DEu) : s_(seed ? seed : 1u) {}
    unsigned NextU() { s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5; return s_; }
    float Next01()             { return (NextU() & 0xFFFFFF) / float(0x1000000); }       // [0,1)
    float Range(float a, float b) { return a + (b - a) * Next01(); }                      // [a,b)
    int   RangeI(int a, int b) { return a + int(Next01() * (b - a + 1)); }                // [a,b]
    Vec3  OnSphere() {
        float u = Range(-1, 1), t = Range(0, TWO_PI);
        float r = std::sqrt(std::max(0.0f, 1 - u * u));
        return {r * std::cos(t), r * std::sin(t), u};
    }
private:
    unsigned s_;
};

// Smooth value noise in 1D (sum of sines with incommensurate frequencies).
// Used for plant sway / lighting drift so nothing ever loops on a fixed period.
inline float DriftNoise(float t, float seed) {
    return 0.55f * std::sin(t * 0.7137f + seed) +
           0.30f * std::sin(t * 1.2719f + seed * 2.3f) +
           0.15f * std::sin(t * 2.5331f + seed * 5.1f);
}

} // namespace nyx
