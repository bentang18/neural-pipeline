#include "science/neural_pipeline/pipeline.h"
#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/producer.h"
#include "science/neural_pipeline/sample.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace science::neural_pipeline {
Pipeline::Pipeline(Config config)
    : config_(config), processor_(Processor::Config{}),
      producer_(Producer::Config{}) {
  buffer_ok_ = buffer_.init(config.buffer_capacity);
}

Pipeline::~Pipeline() { stop(); }

auto Pipeline::start() -> bool {
  if (running_ || !buffer_ok_) {
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
  const size_t batch_size = config_.sample_rate_hz / 1000; // 30 samples per ms
  while (running_) {
    for (size_t i = 0; i < batch_size && running_; ++i) {
      auto now = std::chrono::steady_clock::now();
      Sample sample = producer_.generate(
          std::chrono::duration_cast<std::chrono::microseconds>(
              now.time_since_epoch())
              .count());

      if (buffer_.push(std::move(sample))) {
        samples_produced_.fetch_add(1, std::memory_order_relaxed);
      } else {
        samples_dropped_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void Pipeline::consumer_loop() {
  while (running_ || !buffer_.empty()) {
    Sample sample;
    if (buffer_.pop(sample)) {
      auto now = std::chrono::steady_clock::now();
      uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            now.time_since_epoch())
                            .count();
      uint64_t latency_us = now_us - sample.timestamp_us;

      samples_consumed_.fetch_add(1, std::memory_order_relaxed);
      latency_total_.fetch_add(latency_us, std::memory_order_relaxed);

      if (latency_us > max_latency_us_.load(std::memory_order_relaxed)) {
        max_latency_us_.store(latency_us, std::memory_order_relaxed);
      }

      spikes_detected_.fetch_add(processor_.process(sample).spikes_detected,
                                 std::memory_order_relaxed);
    }
  }
}

auto Pipeline::stats() const -> Stats {
  Stats s;
  s.samples_produced = samples_produced_.load(std::memory_order_relaxed);
  s.samples_consumed = samples_consumed_.load(std::memory_order_relaxed);
  s.samples_dropped = samples_dropped_.load(std::memory_order_relaxed);
  s.spikes_detected = spikes_detected_.load(std::memory_order_relaxed);
  s.max_latency = max_latency_us_.load(std::memory_order_relaxed);
  if (s.samples_consumed > 0) {
    s.avg_latency_us =
        static_cast<double>(latency_total_.load(std::memory_order_relaxed)) /
        static_cast<double>(s.samples_consumed);
  }
  return s;
}

} // namespace science::neural_pipeline