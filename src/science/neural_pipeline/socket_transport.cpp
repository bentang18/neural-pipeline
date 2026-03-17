#include "science/neural_pipeline/socket_transport.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace science::neural_pipeline {

SocketTransport::SocketTransport(Config config) : config_(std::move(config)) {}

SocketTransport::~SocketTransport() { close(); }

auto SocketTransport::listen_and_accept() -> bool {
  listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return false;
  }

  unlink(config_.socket_path.c_str());

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, config_.socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    return false;
  }
  if (listen(listen_fd_, 1) < 0) {
    return false;
  }

  conn_fd_ = accept(listen_fd_, nullptr, nullptr);
  return conn_fd_ >= 0;
}

auto SocketTransport::connect() -> bool {
  conn_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (conn_fd_ < 0) {
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, config_.socket_path.c_str(), sizeof(addr.sun_path) - 1);

  return ::connect(conn_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

auto SocketTransport::send(const Sample& sample) -> bool {
  uint64_t timestamp = sample.timestamp_us;
  uint32_t num_channels = static_cast<uint32_t>(sample.channels.size());

  if (!write_all(&timestamp, sizeof(timestamp))) return false;
  if (!write_all(&num_channels, sizeof(num_channels))) return false;
  if (num_channels > 0) {
    if (!write_all(sample.channels.data(), num_channels * sizeof(float))) return false;
  }
  return true;
}

auto SocketTransport::receive(Sample& sample) -> bool {
  uint64_t timestamp = 0;
  uint32_t num_channels = 0;

  if (!read_all(&timestamp, sizeof(timestamp))) return false;
  if (!read_all(&num_channels, sizeof(num_channels))) return false;

  sample.timestamp_us = timestamp;
  sample.channels.resize(num_channels);
  if (num_channels > 0) {
    if (!read_all(sample.channels.data(), num_channels * sizeof(float))) return false;
  }
  return true;
}

auto SocketTransport::close() -> void {
  if (conn_fd_ >= 0) {
    ::close(conn_fd_);
    conn_fd_ = -1;
  }
  if (listen_fd_ >= 0) {
    ::close(listen_fd_);
    unlink(config_.socket_path.c_str());
    listen_fd_ = -1;
  }
}

auto SocketTransport::write_all(const void* buf, size_t len) -> bool {
  auto* ptr = static_cast<const uint8_t*>(buf);
  size_t remaining = len;
  while (remaining > 0) {
    ssize_t n = ::write(conn_fd_, ptr, remaining);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    ptr += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

auto SocketTransport::read_all(void* buf, size_t len) -> bool {
  auto* ptr = static_cast<uint8_t*>(buf);
  size_t remaining = len;
  while (remaining > 0) {
    ssize_t n = ::read(conn_fd_, ptr, remaining);
    if (n <= 0) {
      if (n < 0 && errno == EINTR) continue;
      return false;  // EOF or error
    }
    ptr += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

}  // namespace science::neural_pipeline
