#include "science/neural_pipeline/producer.h"
#include "science/neural_pipeline/socket_transport.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

using science::neural_pipeline::Producer;
using science::neural_pipeline::SocketTransport;

std::atomic_bool g_shutdown{false};

void signal_handler(int /*signum*/) {
  g_shutdown.store(true, std::memory_order_relaxed);
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGPIPE, SIG_IGN);

  std::string socket_path = "/tmp/neural_pipeline.sock";
  if (argc > 1) {
    socket_path = argv[1];
  }

  SocketTransport transport({socket_path});
  Producer::Config producer_config;
  Producer producer(producer_config);

  std::cout << "producer_node: waiting for connection on " << socket_path
            << "\n";
  if (!transport.listen_and_accept()) {
    std::cerr << "Failed to bind/accept\n";
    return 1;
  }
  std::cout << "producer_node: connected!\n";

  auto start_time = std::chrono::steady_clock::now();
  auto last_print = start_time;
  uint64_t samples_sent = 0;
  size_t batch_size = producer_config.sample_rate_hz / 1000;

  while (!g_shutdown.load(std::memory_order_relaxed)) {
    auto now = std::chrono::steady_clock::now();
    auto timestamp_us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_time)
            .count();

    for (size_t i = 0; i < batch_size; ++i) {
      auto sample = producer.generate(static_cast<uint64_t>(timestamp_us));
      if (!transport.send(sample)) {
        std::cout << "producer_node: connection closed\n";
        goto done;
      }
      ++samples_sent;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - last_print);
    if (elapsed.count() >= 1) {
      std::cout << "producer_node: sent " << samples_sent << " samples\n";
      last_print = now;
    }
  }

done:
  transport.close();
  auto total = std::chrono::steady_clock::now() - start_time;
  double runtime_s = std::chrono::duration<double>(total).count();
  std::cout << "\nproducer_node: " << samples_sent << " samples in "
            << runtime_s << "s\n";
  return 0;
}
