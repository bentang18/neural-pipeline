#include "science/neural_pipeline/pipeline.h"
#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/producer.h"
#include "science/neural_pipeline/sample.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace science::neural_pipeline {
Pipeline::Pipeline(Config config)
    : config_(config), buffer_(config.buffer_capacity),
      processor_(Processor::Config{}), producer_(Producer::Config{}) {}

Pipeline::~Pipeline() { stop(); }

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
    auto now = std::chrono::steady_clock::now();
    Sample sample = producer_.generate(
        std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch())
            .count());
    buffer_.push(std::move(sample));
    std::this_thread::sleep_for(
        std::chrono::microseconds(33)); //~30khz, will fix later
  }
}

void Pipeline::consumer_loop() {
  uint64_t samples_consumed = 0;
  uint64_t latency_total = 0;
  size_t spikes_total = 0;
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

      spikes_total += processor_.process(sample).spikes_detected;

      if (samples_consumed % config_.sample_rate_hz == 0) {
        std::cout << "consumed: " << samples_consumed << " | latency: "
                  << static_cast<double>(latency_total) /
                         static_cast<double>(config_.sample_rate_hz)
                  << " us | spikes detected: " << spikes_total << '\n';

        latency_total = 0;
        spikes_total = 0;
      }
    }
  }
}

} // namespace science::neural_pipeline