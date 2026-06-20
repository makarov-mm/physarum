#include "physarum.hpp"
#include <algorithm>
#include <cmath>
#include <thread>

template <class Fn>
static void parallelFor(int n, int threads, Fn&& fn) {
    if (threads <= 1 || n < 4096) { for (int i = 0; i < n; ++i) fn(i); return; }
    std::vector<std::thread> pool;
    int chunk = (n + threads - 1) / threads;
    for (int t = 0; t < threads; ++t) {
        int b = t * chunk, e = std::min(b + chunk, n);
        if (b >= e) break;
        pool.emplace_back([&fn, b, e]() { for (int i = b; i < e; ++i) fn(i); });
    }
    for (auto& th : pool) th.join();
}

static inline uint32_t xorshift(uint64_t& s) {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return (uint32_t)(s >> 32);
}
static inline float frand(uint64_t& s) { return (xorshift(s) >> 8) * (1.0f / 16777216.0f); }

static Vec3 randUnit(uint64_t& s) {
    // uniform on sphere
    float z = frand(s) * 2.0f - 1.0f;
    float a = frand(s) * 6.2831853f;
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return {r * std::cos(a), r * std::sin(a), z};
}

Physarum::Physarum(const PhysarumParams& p) : p_(p), field_(p.grid) {
    const int n = p_.agents;
    pos_.resize(n); head_.resize(n); trail_.assign(n, 0.0f); rng_.resize(n);
    uint64_t s = p_.seed ? p_.seed : 1;
    float g = (float)p_.grid;
    for (int i = 0; i < n; ++i) {
        rng_[i] = (s += 0x9E3779B97F4A7C15ull) | 1ull;
        uint64_t r = rng_[i];
        // seed agents uniformly across the whole volume so the network spans space
        pos_[i] = Vec3{frand(r) * g, frand(r) * g, frand(r) * g};
        head_[i] = randUnit(r);
        rng_[i] = r;
    }
}

void Physarum::step() {
    const int n = (int)pos_.size();
    const float SO = p_.sensorDist, SA = p_.sensorAngle, TR = p_.turnRate;
    const float SS = p_.stepSize, JIT = p_.jitter;
    const float g = (float)p_.grid;

    // Phase 1 (parallel, read-only field): sense + steer + move.
    parallelFor(n, threads_, [&](int i) {
        Vec3 h = head_[i];
        Vec3 pos = pos_[i];
        // orthonormal frame around heading
        Vec3 ref = (std::fabs(h.y) < 0.9f) ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
        Vec3 u = h.cross(ref).normalized();
        Vec3 w = h.cross(u);

        float cS = std::cos(SA), sS = std::sin(SA);
        Vec3 best = h;
        float bestVal = field_.sample(pos + h * SO);
        // four cone sensors (up/right/down/left relative to heading)
        const float ph[4] = {0.0f, 1.5707963f, 3.1415926f, 4.7123889f};
        for (int k = 0; k < 4; ++k) {
            Vec3 dir = (h * cS + (u * std::cos(ph[k]) + w * std::sin(ph[k])) * sS).normalized();
            float v = field_.sample(pos + dir * SO);
            if (v > bestVal) { bestVal = v; best = dir; }
        }

        uint64_t r = rng_[i];
        Vec3 nh;
        if (bestVal <= 1e-6f) {
            // nothing to follow: wander
            nh = (h + randUnit(r) * 0.5f).normalized();
        } else {
            nh = (h + (best - h) * TR).normalized();
            nh = (nh + randUnit(r) * JIT).normalized();
        }
        rng_[i] = r;

        Vec3 np = pos + nh * SS;
        // wrap into [0, g)
        auto wrapf = [&](float v) { v = std::fmod(v, g); return v < 0 ? v + g : v; };
        np = {wrapf(np.x), wrapf(np.y), wrapf(np.z)};

        head_[i] = nh;
        pos_[i] = np;
        trail_[i] = bestVal;
    });

    // Phase 2 (serial write): deposit. Cheap relative to sensing; avoids races.
    for (int i = 0; i < n; ++i) field_.deposit(pos_[i], p_.deposit);

    // Phase 3: diffuse + decay (parallel).
    field_.diffuseDecay(p_.decay, [&](int cnt, auto&& fn) { parallelFor(cnt, threads_, fn); });
}
