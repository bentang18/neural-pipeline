#pragma once

#include "science/neural_pipeline/producer.h"
#include "science/neural_pipeline/sample.h"
#include <random>

namespace science::neural_pipeline {

Producer::Producer(Config config) : config_(config), rng_(config.seed) {}

auto Producer::generate(uint64_t timestamp_us) -> Sample {
  Sample sample;
  sample.timestamp_us = timestamp_us;
  sample.channels.resize(config_.num_channels);
  std::normal_distribution<float> noise(0.0F, config_.noise_std);
  std::uniform_real_distribution<float> spike_chance(0.0F, 1.0F);
  float spike_probability =
      config_.spike_rate_hz / static_cast<float>(config_.sample_rate_hz);

  for (auto &x : sample.channels) {
    x = noise(rng_);
    if (spike_chance(rng_) < spike_probability) {
      x += -5.0F * config_.noise_std;
    }
  }
  return sample;
}

} // namespace science::neural_pipeline
