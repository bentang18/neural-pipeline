#pragma once

#include <atomic>
#include <thread>

#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/producer.h"
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

  struct Stats {
    uint64_t samples_produced = 0;
    uint64_t samples_consumed = 0;
    uint64_t samples_dropped = 0;
    uint64_t spikes_detected = 0;
    uint64_t max_latency = 0;
    double avg_latency_us = 0.0;
  };

  auto stats() const -> Stats;

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
  bool buffer_ok_ = false;
  std::atomic<bool> running_{false};
  std::thread producer_thread_;
  std::thread consumer_thread_;
  Producer producer_;
  Processor processor_;

  std::atomic<uint64_t> samples_produced_ = {0};
  std::atomic<uint64_t> samples_consumed_ = {0};
  std::atomic<uint64_t> samples_dropped_ = {0};
  std::atomic<uint64_t> spikes_detected_ = {0};
  std::atomic<uint64_t> latency_total_ = {0};
  std::atomic<uint64_t> max_latency_us_ = {0};
};

} // namespace science::neural_pipeline
