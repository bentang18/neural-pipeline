#include "science/neural_pipeline/pipeline.h"
#include "science/neural_pipeline/sample.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace science::neural_pipeline {
Pipeline::Pipeline(Config config)
    : config_(config), buffer_(config.buffer_capacity) {}

Pipeline::~Pipeline() {stop();}

auto Pipeline::start() -> bool {
  if (running_) {
    return false;
  }
  running_.store(true, std::memory_order_relaxed);
  producer_thread_ = std::thread(&Pipeline::producer_loop, this);
  consumer_thread_ = std::thread(&Pipeline::consumer_loop, this);

  return true;
}
auto Pipeline::stop() -> void {
  if (!running_) {
    return;
  }
  running_.store(false, std::memory_order_relaxed);
  producer_thread_.join();
  consumer_thread_.join();
}
void Pipeline::producer_loop() {
  while (running_) {
    Sample sample;
    auto now = std::chrono::steady_clock::now();
    sample.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                              .count();
    sample.channels = std::vector<float>(config_.num_channels, 0.0F);
    buffer_.push(std::move(sample));
    std::this_thread::sleep_for(
        std::chrono::microseconds(33)); //~30khz, will fix later
  }
}
void Pipeline::consumer_loop() {
  uint64_t samples_consumed = 0;
  uint64_t latency_total = 0;
  while (running_) {
    Sample sample;
    if (buffer_.pop(sample)) {
      auto now = std::chrono::steady_clock::now();
      uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            now.time_since_epoch())
                            .count();
      uint64_t latency_us = now_us - sample.timestamp_us;
      samples_consumed++;
      latency_total += latency_us;
      if (samples_consumed % config_.sample_rate_hz == 0) {
        std::cout << "consumed: " << samples_consumed << " | latency: "
                  << static_cast<double>(latency_total) / config_.sample_rate_hz
                  << " us\n";

        latency_total = 0;
      }
    }
  }
}

} // namespace science::neural_pipeline