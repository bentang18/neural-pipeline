#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/shared_sample.h"
#include "science/neural_pipeline/shm_transport.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using science::neural_pipeline::Processor;
using science::neural_pipeline::ShmTransport;

std::atomic_bool g_shutdown{false};

void signal_handler(int /*signum*/) {
  g_shutdown.store(true, std::memory_order_relaxed);
}

int main() {
  std::signal(SIGINT, signal_handler);

  ShmTransport::Config shm_cfg;
  shm_cfg.name = "/neural_pipeline";
  shm_cfg.capacity = 4096;

  ShmTransport transport(shm_cfg);

  std::cout << "processor_node: waiting for producer..." << std::endl;
  while (!transport.open() && !g_shutdown.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if (g_shutdown.load(std::memory_order_relaxed)) {
    return 0;
  }
  std::cout << "processor_node: connected to shared memory" << std::endl;

  Processor::Config proc_cfg;
  Processor processor(proc_cfg);
  float threshold = -proc_cfg.threshold_factor * proc_cfg.noise_std;

  uint64_t samples_consumed = 0;
  uint64_t spikes_total = 0;
  uint64_t latency_total = 0;
  uint64_t max_latency = 0;

  uint64_t prev_consumed = 0;
  uint64_t prev_latency = 0;
  auto start_time = std::chrono::steady_clock::now();
  auto last_print = start_time;

  while (!g_shutdown.load(std::memory_order_relaxed)) {
    const auto* slot = transport.read_slot();
    if (slot == nullptr) {
      std::this_thread::yield();
      continue;
    }

    // Compute latency (same clock domain — steady_clock on both sides)
    auto now = std::chrono::steady_clock::now();
    uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          now.time_since_epoch())
                          .count();
    uint64_t latency_us = now_us - slot->timestamp_us;

    // Spike detection — read directly from shared memory (zero-copy read!)
    size_t spikes = 0;
    for (uint32_t i = 0; i < slot->num_channels; ++i) {
      spikes += (slot->channels[i] < threshold);
    }

    transport.consume();

    ++samples_consumed;
    spikes_total += spikes;
    latency_total += latency_us;
    if (latency_us > max_latency) {
      max_latency = latency_us;
    }

    if (now - last_print >= std::chrono::seconds(1)) {
      double elapsed_s = std::chrono::duration<double>(now - start_time).count();
      uint64_t interval_samples = samples_consumed - prev_consumed;
      double interval_avg = interval_samples > 0
                                ? static_cast<double>(latency_total - prev_latency) /
                                      static_cast<double>(interval_samples)
                                : 0.0;
      std::cout << "[" << static_cast<int>(elapsed_s) << "s]"
                << " consumed: " << samples_consumed
                << " | spikes: " << spikes_total
                << " | latency: " << interval_avg << "us avg, "
                << max_latency << "us max" << std::endl;
      prev_consumed = samples_consumed;
      prev_latency = latency_total;
      max_latency = 0;
      last_print = now;
    }
  }

  std::cout << "\nprocessor_node stopped. Total consumed: " << samples_consumed
            << " | spikes: " << spikes_total << "\n";
  transport.close();
  return 0;
}
