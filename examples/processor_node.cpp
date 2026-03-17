#include "science/neural_pipeline/processor.h"
#include "science/neural_pipeline/socket_transport.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>

using science::neural_pipeline::Processor;
using science::neural_pipeline::Sample;
using science::neural_pipeline::SocketTransport;

std::atomic_bool g_shutdown{false};

void signal_handler(int /*signum*/) { g_shutdown.store(true, std::memory_order_relaxed); }

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signal_handler);

  std::string socket_path = "/tmp/neural_pipeline.sock";
  if (argc > 1) {
    socket_path = argv[1];
  }

  SocketTransport transport({socket_path});
  Processor::Config processor_config;
  Processor processor(processor_config);

  std::cout << "processor_node: connecting to " << socket_path << "\n";
  if (!transport.connect()) {
    std::cerr << "Failed to connect\n";
    return 1;
  }
  std::cout << "processor_node: connected!\n";

  auto start_time = std::chrono::steady_clock::now();
  auto last_print = start_time;
  uint64_t samples_received = 0;
  uint64_t spikes_detected = 0;
  uint64_t latency_total_us = 0;
  uint64_t max_latency_us = 0;

  Sample sample;
  while (!g_shutdown.load(std::memory_order_relaxed)) {
    if (!transport.receive(sample)) {
      std::cout << "processor_node: producer disconnected\n";
      break;
    }

    auto now = std::chrono::steady_clock::now();
    auto now_us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
    uint64_t latency_us = static_cast<uint64_t>(now_us) - sample.timestamp_us;

    auto result = processor.process(sample);
    ++samples_received;
    spikes_detected += result.spikes_detected;
    latency_total_us += latency_us;
    if (latency_us > max_latency_us) {
      max_latency_us = latency_us;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_print);
    if (elapsed.count() >= 1) {
      uint64_t avg_latency = samples_received > 0 ? latency_total_us / samples_received : 0;
      std::cout << "[" << std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count()
                << "s]"
                << " received: " << samples_received << " | spikes: " << spikes_detected
                << " | latency: " << avg_latency << "us avg, " << max_latency_us << "us max\n";
      last_print = now;
    }
  }

  transport.close();
  auto total = std::chrono::steady_clock::now() - start_time;
  double runtime_s = std::chrono::duration<double>(total).count();
  std::cout << "\nFinal stats:\n";
  std::cout << "  Total samples:   " << samples_received << "\n";
  std::cout << "  Spikes detected: " << spikes_detected << "\n";
  if (samples_received > 0) {
    std::cout << "  Avg latency:     " << latency_total_us / samples_received << " us\n";
  }
  std::cout << "  Max latency:     " << max_latency_us << " us\n";
  if (runtime_s > 0) {
    std::cout << "  Spike rate:      " << static_cast<double>(spikes_detected) / runtime_s
              << " Hz\n";
  }
  return 0;
}
