#include "science/neural_pipeline/shm_transport.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <new>

namespace science::neural_pipeline {

ShmTransport::ShmTransport(Config config) : config_(std::move(config)) {}

ShmTransport::~ShmTransport() { close(); }

auto ShmTransport::create() -> bool {
  // Validate capacity is power of 2
  if (config_.capacity == 0 || (config_.capacity & (config_.capacity - 1)) != 0) {
    return false;
  }

  shm_size_ = sizeof(ShmHeader) + config_.capacity * sizeof(SharedSample);

  // Remove stale segment (ignore error if it doesn't exist)
  shm_unlink(config_.name.c_str());

  fd_ = shm_open(config_.name.c_str(), O_CREAT | O_RDWR, 0666);
  if (fd_ < 0) {
    return false;
  }

  if (ftruncate(fd_, static_cast<off_t>(shm_size_)) != 0) {
    ::close(fd_);
    fd_ = -1;
    return false;
  }

  base_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (base_ == MAP_FAILED) {
    ::close(fd_);
    fd_ = -1;
    base_ = nullptr;
    return false;
  }

  // Placement new: construct ShmHeader (including atomics) in raw mmap'd memory
  header_ = new (base_) ShmHeader{};
  header_->capacity = config_.capacity;
  header_->max_channels = config_.max_channels;
  header_->write_pos.store(0, std::memory_order_relaxed);
  header_->read_pos.store(0, std::memory_order_relaxed);

  // Slots start right after the header
  slots_ = reinterpret_cast<SharedSample*>(static_cast<char*>(base_) + sizeof(ShmHeader));
  mask_ = config_.capacity - 1;
  is_server_ = true;

  return true;
}

auto ShmTransport::open() -> bool {
  fd_ = shm_open(config_.name.c_str(), O_RDWR, 0);
  if (fd_ < 0) {
    return false;
  }

  // Use fstat to get the real size — don't trust config
  struct stat st {};
  if (fstat(fd_, &st) != 0) {
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  shm_size_ = static_cast<size_t>(st.st_size);

  base_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (base_ == MAP_FAILED) {
    ::close(fd_);
    fd_ = -1;
    base_ = nullptr;
    return false;
  }

  // NO placement new — server already initialized the header
  header_ = static_cast<ShmHeader*>(base_);

  // Validate that the server's config matches ours
  if (header_->capacity != config_.capacity || header_->max_channels != config_.max_channels) {
    close();
    return false;
  }

  slots_ = reinterpret_cast<SharedSample*>(static_cast<char*>(base_) + sizeof(ShmHeader));
  mask_ = header_->capacity - 1;
  is_server_ = false;

  return true;
}

auto ShmTransport::write_slot() -> SharedSample* {
  auto wp = header_->write_pos.load(std::memory_order_relaxed);
  auto rp = header_->read_pos.load(std::memory_order_acquire);
  if (wp - rp >= header_->capacity) {
    return nullptr;  // full
  }
  return &slots_[wp & mask_];
}

void ShmTransport::publish() {
  auto wp = header_->write_pos.load(std::memory_order_relaxed);
  header_->write_pos.store(wp + 1, std::memory_order_release);
}

auto ShmTransport::read_slot() -> const SharedSample* {
  auto rp = header_->read_pos.load(std::memory_order_relaxed);
  auto wp = header_->write_pos.load(std::memory_order_acquire);
  if (rp >= wp) {
    return nullptr;  // empty
  }
  return &slots_[rp & mask_];
}

void ShmTransport::consume() {
  auto rp = header_->read_pos.load(std::memory_order_relaxed);
  header_->read_pos.store(rp + 1, std::memory_order_release);
}

void ShmTransport::close() {
  if (base_ != nullptr) {
    munmap(base_, shm_size_);
    base_ = nullptr;
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  // Only the server unlinks the name
  if (is_server_) {
    shm_unlink(config_.name.c_str());
    is_server_ = false;
  }
  header_ = nullptr;
  slots_ = nullptr;
}

}  // namespace science::neural_pipeline
