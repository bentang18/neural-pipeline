#pragma once

#include <cstdint>
#include <random>

#include "science/neural_pipeline/sample.h"

namespace science::neural_pipeline {

class Producer {
public:
  struct Config {
    size_t num_channels = 32;
    size_t sample_rate_hz = 30000;
    float noise_std = 1.0F;
    float spike_rate_hz = 5.0F; // spikes per second per channel
    uint64_t seed = 42;         // deterministic for testing
  };

  explicit Producer(Config config);
  auto generate(uint64_t timestamp_us) -> Sample;

private:
  Config config_;
  std::mt19937 rng_;
};

} // namespace science::neural_pipeline