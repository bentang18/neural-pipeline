#include "science/neural_pipeline/shm_transport.h"

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>

using science::neural_pipeline::ShmTransport;

int main() {
  constexpr uint64_t kCount = 5'000'000;

  ShmTransport::Config cfg;
  cfg.name = "/shm_bench";
  cfg.capacity = 4096;

  ShmTransport server(cfg);
  if (!server.create()) {
    std::cerr << "create failed\n";
    return 1;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Child = consumer: spin as fast as possible
    ShmTransport client(cfg);
    if (!client.open()) _exit(1);

    for (uint64_t i = 0; i < kCount; ++i) {
      while (client.read_slot() == nullptr) {}
      client.consume();
    }
    _exit(0);
  }

  // Parent = producer: spin as fast as possible
  auto start = std::chrono::steady_clock::now();

  for (uint64_t i = 0; i < kCount; ++i) {
    while (server.write_slot() == nullptr) {}
    auto* slot = server.write_slot();
    slot->timestamp_us = i;
    slot->num_channels = 32;
    std::memset(slot->channels, 0, 32 * sizeof(float));
    server.publish();
  }

  int status = 0;
  waitpid(pid, &status, 0);
  auto end = std::chrono::steady_clock::now();

  double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
  double ns_per_sample = (elapsed_ms * 1e6) / static_cast<double>(kCount);

  std::cout << kCount << " samples in " << elapsed_ms << " ms\n";
  std::cout << ns_per_sample << " ns/sample\n";
  std::cout << static_cast<uint64_t>(kCount / (elapsed_ms / 1000.0)) << " samples/sec\n";

  server.close();
  return 0;
}
