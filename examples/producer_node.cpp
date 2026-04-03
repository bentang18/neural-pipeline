#include "science/neural_pipeline/producer.h"
#include "science/neural_pipeline/shared_sample.h"
#include "science/neural_pipeline/shm_transport.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <csignal>
#include <iostream>
#include <thread>

using science::neural_pipeline::Producer;
using science::neural_pipeline::ShmTransport;
using science::neural_pipeline::MAX_CHANNELS;

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
  if (!transport.create()) {
    std::cerr << "Failed to create shared memory\n";
    return 1;
  }

  Producer::Config prod_cfg;
  Producer producer(prod_cfg);

  std::cout << "producer_node started (shm: " << shm_cfg.name << ")" << std::endl;
  std::cout << "channels: " << prod_cfg.num_channels
            << " | capacity: " << shm_cfg.capacity << std::endl;

  const size_t batch_size = prod_cfg.sample_rate_hz / 1000;  // samples per ms
  uint64_t samples_produced = 0;
  auto start_time = std::chrono::steady_clock::now();
  auto last_print = start_time;

  while (!g_shutdown.load(std::memory_order_relaxed)) {
    for (size_t i = 0; i < batch_size && !g_shutdown.load(std::memory_order_relaxed); ++i) {
      auto* slot = transport.write_slot();
      if (slot == nullptr) {
        std::this_thread::yield();
        continue;
      }

      auto now = std::chrono::steady_clock::now();
      uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                               now.time_since_epoch())
                               .count();

      auto sample = producer.generate(timestamp);

      slot->timestamp_us = sample.timestamp_us;
      slot->num_channels = static_cast<uint32_t>(sample.channels.size());
      if (slot->num_channels > MAX_CHANNELS) {
        slot->num_channels = MAX_CHANNELS;
      }
      std::memcpy(slot->channels, sample.channels.data(),
                   slot->num_channels * sizeof(float));

      transport.publish();  // O(1) IPC — one atomic store
      ++samples_produced;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto now = std::chrono::steady_clock::now();
    if (now - last_print >= std::chrono::seconds(1)) {
      auto elapsed_s = std::chrono::duration<double>(now - start_time).count();
      std::cout << "[" << static_cast<int>(elapsed_s) << "s]"
                << " produced: " << samples_produced << std::endl;
      last_print = now;
    }
  }

  std::cout << "\nproducer_node stopped. Total produced: " << samples_produced << "\n";
  transport.close();
  return 0;
}
