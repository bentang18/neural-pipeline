#include "science/neural_pipeline/pipeline.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using science::neural_pipeline::Pipeline;

std::atomic_bool g_shutdown{false};

void signal_handler(int /*signum*/) {
  g_shutdown.store(true, std::memory_order_relaxed);
}

int main() {
  Pipeline::Config config;
  Pipeline pipeline(config);

  std::signal(SIGINT, signal_handler);

  std::cout << "neural-pipeline demo\n";
  std::cout << "====================\n";
  std::cout << "Config: " << config.num_channels << " channels @ "
            << config.sample_rate_hz << " Hz, buffer capacity: "
            << config.buffer_capacity << "\n\n";

  if (!pipeline.start()) {
    std::cerr << "Failed to start pipeline\n";
    return 1;
  }
  auto start_time = std::chrono::steady_clock::now();

  while (!g_shutdown.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (g_shutdown.load(std::memory_order_relaxed)) break;
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    auto s = pipeline.stats();
    std::cout << "[" << seconds << "s]"
              << " produced: " << s.samples_produced
              << " | consumed: " << s.samples_consumed
              << " | dropped: " << s.samples_dropped
              << " | spikes: " << s.spikes_detected
              << " | latency: " << s.avg_latency_us << "us avg, "
              << s.max_latency << "us max\n";
  }

  pipeline.stop();

  auto s = pipeline.stats();
  auto elapsed = std::chrono::steady_clock::now() - start_time;
  double runtime_s = std::chrono::duration<double>(elapsed).count();
  std::cout << "\nFinal stats:\n";
  std::cout << "  Total samples:   " << s.samples_consumed << "\n";
  std::cout << "  Spikes detected: " << s.spikes_detected << "\n";
  std::cout << "  Samples dropped: " << s.samples_dropped << "\n";
  std::cout << "  Avg latency:     " << s.avg_latency_us << " us\n";
  std::cout << "  Max latency:     " << s.max_latency << " us\n";
  if (runtime_s > 0) {
    std::cout << "  Spike rate:      "
              << static_cast<double>(s.spikes_detected) / runtime_s
              << " Hz (expected: ~160 Hz)\n";
  }

  return 0;
}
