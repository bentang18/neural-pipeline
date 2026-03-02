#pragma once

#include <atomic>
#include <stdexcept>
#include <vector>

namespace science::neural_pipeline {

template <typename T> class RingBuffer {
public:
  // Construct with given capacity (must be power of 2)
  explicit RingBuffer(size_t capacity)
      : capacity_(capacity), mask_(capacity - 1), buffer_(capacity),
        write_pos_(0), read_pos_(0) {
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
      throw std::invalid_argument("capacity must be power of 2");
    }
  }

  // Non-copyable, non-movable (atomics can't be copied/moved)
  RingBuffer(const RingBuffer &) = delete;
  RingBuffer &operator=(const RingBuffer &) = delete;

  // Try to push an item. Returns false if full.
  auto push(const T &item) -> bool {
    if (full()) {
      return false;
    }
    buffer_[write_pos_.load(std::memory_order_relaxed) & mask_] = item;

    write_pos_.store(write_pos_.load(std::memory_order_relaxed) + 1,
                     std::memory_order_release);
    return true;
  }

  // pushing a pointer - faster than copying
  auto push(T &&item) -> bool {
    if (full()) {
      return false;
    }

    buffer_[write_pos_.load(std::memory_order_relaxed) & mask_] =
        std::move(item);

    write_pos_.store(write_pos_.load(std::memory_order_relaxed) + 1,
                     std::memory_order_release);
    return true;
  }

  // Try to pop an item into `out`. Returns false if empty.
  auto pop(T &out) -> bool {
    if (empty()) {
      return false;
    }
    out = buffer_[read_pos_.load(std::memory_order_relaxed) & mask_];
    read_pos_.store(read_pos_.load(std::memory_order_relaxed) + 1,
                    std::memory_order_release);
    return true;
  }

  // Current number of items in the buffer
  auto size() const -> size_t {
    return write_pos_.load(std::memory_order_acquire) -
           read_pos_.load(std::memory_order_acquire);
  }

  auto capacity() const -> size_t { return capacity_; }
  auto empty() const -> bool { return (size() == 0); }
  auto full() const -> bool { return (size() == capacity()); }

private:
  size_t capacity_;
  size_t mask_;
  std::vector<T> buffer_;
  alignas(64) std::atomic<size_t> write_pos_;
  alignas(64) std::atomic<size_t> read_pos_;
};

} // namespace science::neural_pipeline
