#pragma once
#include "field3d.hpp"
#include "vec3.hpp"
#include <cstdint>
#include <vector>

struct PhysarumParams {
    int grid = 128;            // field is grid^3
    int agents = 1'000'000;
    float sensorAngle = 0.45f; // radians, half-cone of the side sensors
    float sensorDist = 9.0f;   // how far ahead agents look (grid cells)
    float turnRate = 0.35f;    // how hard they steer toward the best sensor
    float stepSize = 1.0f;     // move per step (grid cells)
    float deposit = 5.0f;      // trail dropped per agent per step
    float decay = 0.90f;       // trail multiplier after diffusion
    float jitter = 0.10f;      // random heading wobble
    uint64_t seed = 1234567;
};

class Physarum {
public:
    explicit Physarum(const PhysarumParams& p);

    void step();

    int gridSize() const { return field_.size(); }
    std::size_t agentCount() const { return pos_.size(); }
    const std::vector<Vec3>& positions() const { return pos_; }
    const Field3D& field() const { return field_; }
    // trail intensity sampled at each agent (filled during step, for colouring)
    const std::vector<float>& agentTrail() const { return trail_; }
    void setThreads(int t) { threads_ = t < 1 ? 1 : t; }

private:
    PhysarumParams p_;
    Field3D field_;
    int threads_ = 1;
    std::vector<Vec3> pos_, head_;
    std::vector<float> trail_;
    std::vector<uint64_t> rng_;
};
