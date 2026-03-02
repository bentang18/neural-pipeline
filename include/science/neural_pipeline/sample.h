#pragma once

#include <cstdint>
#include <vector>

namespace science::neural_pipeline {

struct Sample {
  uint64_t timestamp_us;       // microseconds since pipeline start
  std::vector<float> channels; // one float per channel
};

} // namespace science::neural_pipeline