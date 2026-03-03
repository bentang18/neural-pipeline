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

  pipeline.start();

  int seconds = 0;
  while (!g_shutdown.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (g_shutdown.load(std::memory_order_relaxed)) break;
    seconds++;
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
  double runtime_s = static_cast<double>(seconds);
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
