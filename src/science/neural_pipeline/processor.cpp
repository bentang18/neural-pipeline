#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/sample.h"

namespace science::neural_pipeline {

Processor::Processor(Config config)
    : config_(config), threshold_(-config.threshold_factor * config.noise_std) {
}

auto Processor::process(const Sample &sample) -> Result {
  Result result;
  for (auto x : sample.channels) {
    result.spikes_detected += (x < threshold_);
  }
  return result;
}

} // namespace science::neural_pipeline
