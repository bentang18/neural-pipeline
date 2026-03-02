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
  std::cout << "pipeline started\n";
  std::signal(SIGINT, signal_handler);
  Pipeline pipeline(Pipeline::Config{});

  pipeline.start();
  while (!g_shutdown.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << "\npipeline stopped\n";
  pipeline.stop();

  return 0;
}
