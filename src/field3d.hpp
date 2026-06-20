#pragma once
#include "vec3.hpp"
#include <vector>
#include <cmath>

// 3D scalar trail field on a periodic (toroidal) grid of size G^3.
class Field3D {
public:
    explicit Field3D(int g) : g_(g), data_(static_cast<size_t>(g) * g * g, 0.0f),
                              tmp_(static_cast<size_t>(g) * g * g, 0.0f) {}

    int size() const { return g_; }
    const std::vector<float>& data() const { return data_; }

    static int wrap(int i, int g) { i %= g; return i < 0 ? i + g : i; }

    inline size_t idx(int x, int y, int z) const {
        return (static_cast<size_t>(wrap(z, g_)) * g_ + wrap(y, g_)) * g_ + wrap(x, g_);
    }

    // Trilinear sample at continuous grid coords (wraps).
    float sample(const Vec3& p) const {
        float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
        int x0 = (int)fx, y0 = (int)fy, z0 = (int)fz;
        float tx = p.x - fx, ty = p.y - fy, tz = p.z - fz;
        auto at = [&](int dx, int dy, int dz) { return data_[idx(x0 + dx, y0 + dy, z0 + dz)]; };
        float c00 = at(0,0,0)*(1-tx) + at(1,0,0)*tx;
        float c10 = at(0,1,0)*(1-tx) + at(1,1,0)*tx;
        float c01 = at(0,0,1)*(1-tx) + at(1,0,1)*tx;
        float c11 = at(0,1,1)*(1-tx) + at(1,1,1)*tx;
        float c0 = c00*(1-ty) + c10*ty;
        float c1 = c01*(1-ty) + c11*ty;
        return c0*(1-tz) + c1*tz;
    }

    void deposit(const Vec3& p, float amount) {
        int x = wrap((int)std::lround(p.x), g_);
        int y = wrap((int)std::lround(p.y), g_);
        int z = wrap((int)std::lround(p.z), g_);
        data_[(static_cast<size_t>(z) * g_ + y) * g_ + x] += amount;
    }

    // Separable binomial blur (0.25, 0.5, 0.25) along each axis, then *decay.
    // `pf` is a parallel-for: pf(n, fn).
    template <class ParallelFor>
    void diffuseDecay(float decay, ParallelFor&& pf) {
        const int g = g_;
        auto blurX = [&](const std::vector<float>& in, std::vector<float>& out) {
            pf(g * g, [&](int row) {
                int y = row % g, z = row / g;
                size_t base = (static_cast<size_t>(z) * g + y) * g;
                for (int x = 0; x < g; ++x) {
                    float l = in[base + wrap(x - 1, g)];
                    float c = in[base + x];
                    float r = in[base + wrap(x + 1, g)];
                    out[base + x] = 0.25f * l + 0.5f * c + 0.25f * r;
                }
            });
        };
        // X
        blurX(data_, tmp_);
        // Y (gather along y)
        pf(g * g, [&](int col) {
            int x = col % g, z = col / g;
            for (int y = 0; y < g; ++y) {
                float l = tmp_[(static_cast<size_t>(z) * g + wrap(y - 1, g)) * g + x];
                float c = tmp_[(static_cast<size_t>(z) * g + y) * g + x];
                float r = tmp_[(static_cast<size_t>(z) * g + wrap(y + 1, g)) * g + x];
                data_[(static_cast<size_t>(z) * g + y) * g + x] = 0.25f * l + 0.5f * c + 0.25f * r;
            }
        });
        // Z (gather along z), fold in decay here
        pf(g * g, [&](int col) {
            int x = col % g, y = col / g;
            for (int z = 0; z < g; ++z) {
                float l = data_[(static_cast<size_t>(wrap(z - 1, g)) * g + y) * g + x];
                float c = data_[(static_cast<size_t>(z) * g + y) * g + x];
                float r = data_[(static_cast<size_t>(wrap(z + 1, g)) * g + y) * g + x];
                tmp_[(static_cast<size_t>(z) * g + y) * g + x] = (0.25f * l + 0.5f * c + 0.25f * r) * decay;
            }
        });
        data_.swap(tmp_);
    }

private:
    int g_;
    std::vector<float> data_, tmp_;
};
