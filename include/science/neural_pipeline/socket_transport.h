#pragma once

#include <string>

#include "science/neural_pipeline/sample.h"

namespace science::neural_pipeline {

class SocketTransport {
public:
  struct Config {
    std::string socket_path = "/tmp/neural_pipeline.sock";
  };

  explicit SocketTransport(Config config);
  ~SocketTransport();

  SocketTransport(const SocketTransport &) = delete;
  auto operator=(const SocketTransport &) -> SocketTransport & = delete;

  [[nodiscard]] auto listen_and_accept() -> bool;
  [[nodiscard]] auto connect() -> bool;
  [[nodiscard]] auto send(const Sample &sample) -> bool;
  [[nodiscard]] auto receive(Sample &sample) -> bool;
  auto close() -> void;

private:
  [[nodiscard]] auto write_all(const void *buf, size_t len) -> bool;
  [[nodiscard]] auto read_all(void *buf, size_t len) -> bool;

  Config config_;
  int listen_fd_ = -1;
  int conn_fd_ = -1;
};

} // namespace science::neural_pipeline
