#pragma once

#include <atomic>
#include <thread>

#include "science/neural_pipeline/ring_buffer.h"
#include "science/neural_pipeline/sample.h"

namespace science::neural_pipeline {

class Pipeline {
public:
  struct Config {
    size_t buffer_capacity = 4096;
    size_t num_channels = 32;
    size_t sample_rate_hz = 30000;
  };

  explicit Pipeline(Config config);
  ~Pipeline(); // must join threads

  Pipeline(const Pipeline &) = delete;
  Pipeline &operator=(const Pipeline &) = delete;

  auto start() -> bool;
  auto stop() -> void;

private:
  void producer_loop();
  void consumer_loop();

  Config config_;
  RingBuffer<Sample> buffer_;
  std::atomic<bool> running_{false};
  std::thread producer_thread_;
  std::thread consumer_thread_;
};

} // namespace science::neural_pipeline
