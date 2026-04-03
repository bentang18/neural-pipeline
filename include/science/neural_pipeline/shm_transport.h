#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "science/neural_pipeline/shared_sample.h"

namespace science::neural_pipeline {

// Shared memory header — 3 cache lines, explicit layout.
// Cache line 0: immutable config (set once by server)
// Cache line 1: write_pos (producer-owned)
// Cache line 2: read_pos (consumer-owned)
struct ShmHeader {
  uint32_t capacity;
  uint32_t max_channels;
  char pad_[64 - 2 * sizeof(uint32_t)];

  alignas(64) std::atomic<uint64_t> write_pos;
  alignas(64) std::atomic<uint64_t> read_pos;
};

static_assert(offsetof(ShmHeader, write_pos) == 64);
static_assert(offsetof(ShmHeader, read_pos) == 128);
static_assert(sizeof(ShmHeader) == 192);  // 3 cache lines
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "uint64_t atomics must be lock-free for cross-process use");

class ShmTransport {
 public:
  struct Config {
    std::string name = "/neural_pipeline";  // shm name (must start with /)
    uint32_t capacity = 4096;               // number of slots (must be power of 2)
    uint32_t max_channels = MAX_CHANNELS;
  };

  explicit ShmTransport(Config config);
  ~ShmTransport();

  // Non-copyable, non-movable
  ShmTransport(const ShmTransport&) = delete;
  ShmTransport& operator=(const ShmTransport&) = delete;

  // Server: create shared memory, initialize header
  auto create() -> bool;

  // Client: open existing shared memory
  auto open() -> bool;

  // Producer: get pointer to next writable slot (nullptr if full)
  // Must call publish() after writing. Strict pairing: one write_slot() per publish().
  auto write_slot() -> SharedSample*;

  // Producer: make the last written slot visible to the consumer. O(1) — one atomic store.
  void publish();

  // Consumer: get pointer to next readable slot (nullptr if empty)
  // Must call consume() after reading. Strict pairing: one read_slot() per consume().
  auto read_slot() -> const SharedSample*;

  // Consumer: advance read position. O(1) — one atomic store.
  void consume();

  // Cleanup: unmap memory, close fd, unlink name (server only)
  void close();

 private:
  Config config_;
  int fd_ = -1;
  void* base_ = nullptr;
  size_t shm_size_ = 0;
  bool is_server_ = false;
  ShmHeader* header_ = nullptr;
  SharedSample* slots_ = nullptr;
  uint32_t mask_ = 0;
};

}  // namespace science::neural_pipeline
