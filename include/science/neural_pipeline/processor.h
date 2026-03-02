#include "science/neural_pipeline/sample.h"

namespace science::neural_pipeline {

class Processor {
public:
  struct Config {
    float threshold_factor = 4.0F;
    float noise_std = 1.0F;
  };

  struct Result {
    size_t spikes_detected = 0;
  };

  explicit Processor(Config config);
  auto process(const Sample &sample) -> Result;

private:
  Config config_;
  float threshold_; // = -threshold_factor * noise_std
};

} // namespace science::neural_pipeline
