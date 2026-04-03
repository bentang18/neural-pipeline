#pragma once

#include <cstdint>
#include <type_traits>

namespace science::neural_pipeline {

constexpr uint32_t MAX_CHANNELS = 64;

struct SharedSample {
  uint64_t timestamp_us;       // microseconds since pipeline start
  uint32_t num_channels;
  float channels[MAX_CHANNELS];
  // sizeof(SharedSample) == 272, not 268: compiler adds 4 bytes tail padding
  // so the struct size is a multiple of alignof(uint64_t) == 8
};

static_assert(std::is_trivially_copyable_v<SharedSample>);

} // namespace science::neural_pipeline