#include "science/neural_pipeline/shm_transport.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <thread>

#include <gtest/gtest.h>

using science::neural_pipeline::ShmTransport;

class ShmTransportTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Clean up any leftover shared memory
    shm_unlink(name_.c_str());
  }
  std::string name_ = "/test_shm";
};

TEST_F(ShmTransportTest, CreateAndOpen) {
  ShmTransport::Config cfg{.name = name_, .capacity = 16};
  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  ShmTransport client(cfg);
  ASSERT_TRUE(client.open());
}

TEST_F(ShmTransportTest, WriteAndRead) {
  ShmTransport::Config cfg{.name = name_, .capacity = 16};
  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  auto* slot = server.write_slot();
  ASSERT_NE(slot, nullptr);
  slot->timestamp_us = 42;
  slot->num_channels = 4;
  slot->channels[0] = 1.0F;
  slot->channels[1] = 2.0F;
  slot->channels[2] = 3.0F;
  slot->channels[3] = 4.0F;
  server.publish();

  ShmTransport client(cfg);
  ASSERT_TRUE(client.open());

  const auto* read = client.read_slot();
  ASSERT_NE(read, nullptr);
  EXPECT_EQ(read->timestamp_us, 42U);
  EXPECT_EQ(read->num_channels, 4U);
  EXPECT_FLOAT_EQ(read->channels[0], 1.0F);
  EXPECT_FLOAT_EQ(read->channels[1], 2.0F);
  EXPECT_FLOAT_EQ(read->channels[2], 3.0F);
  EXPECT_FLOAT_EQ(read->channels[3], 4.0F);
  client.consume();
}

TEST_F(ShmTransportTest, EmptyReturnsNull) {
  ShmTransport::Config cfg{.name = name_, .capacity = 16};
  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  ShmTransport client(cfg);
  ASSERT_TRUE(client.open());

  EXPECT_EQ(client.read_slot(), nullptr);
}

TEST_F(ShmTransportTest, FullReturnsNull) {
  ShmTransport::Config cfg{.name = name_, .capacity = 4};
  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  for (int i = 0; i < 4; ++i) {
    auto* slot = server.write_slot();
    ASSERT_NE(slot, nullptr);
    slot->timestamp_us = static_cast<uint64_t>(i);
    server.publish();
  }

  EXPECT_EQ(server.write_slot(), nullptr);
}

TEST_F(ShmTransportTest, MultiSampleRoundTrip) {
  ShmTransport::Config cfg{.name = name_, .capacity = 128};
  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  ShmTransport client(cfg);
  ASSERT_TRUE(client.open());

  constexpr int kCount = 100;
  for (int i = 0; i < kCount; ++i) {
    auto* slot = server.write_slot();
    ASSERT_NE(slot, nullptr);
    slot->timestamp_us = static_cast<uint64_t>(i);
    slot->num_channels = 2;
    slot->channels[0] = static_cast<float>(i) * 1.0F;
    slot->channels[1] = static_cast<float>(i) * 2.0F;
    server.publish();
  }

  for (int i = 0; i < kCount; ++i) {
    const auto* slot = client.read_slot();
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->timestamp_us, static_cast<uint64_t>(i));
    EXPECT_EQ(slot->num_channels, 2U);
    EXPECT_FLOAT_EQ(slot->channels[0], static_cast<float>(i) * 1.0F);
    EXPECT_FLOAT_EQ(slot->channels[1], static_cast<float>(i) * 2.0F);
    client.consume();
  }
}

TEST_F(ShmTransportTest, CrossProcess) {
  ShmTransport::Config cfg{.name = name_, .capacity = 64};
  constexpr int kCount = 50;

  ShmTransport server(cfg);
  ASSERT_TRUE(server.create());

  pid_t pid = fork();
  ASSERT_NE(pid, -1);

  if (pid == 0) {
    // Child = consumer
    ShmTransport client(cfg);
    if (!client.open()) {
      _exit(1);
    }

    for (int i = 0; i < kCount; ++i) {
      const science::neural_pipeline::SharedSample* slot = nullptr;
      while ((slot = client.read_slot()) == nullptr) {
        std::this_thread::yield();
      }
      if (slot->timestamp_us != static_cast<uint64_t>(i)) {
        _exit(1);
      }
      client.consume();
    }
    _exit(0);  // _exit, not exit — avoid GTest atexit handlers
  }

  // Parent = producer
  for (int i = 0; i < kCount; ++i) {
    auto* slot = server.write_slot();
    while (slot == nullptr) {
      std::this_thread::yield();
      slot = server.write_slot();
    }
    slot->timestamp_us = static_cast<uint64_t>(i);
    slot->num_channels = 1;
    slot->channels[0] = static_cast<float>(i);
    server.publish();
  }

  int status = 0;
  waitpid(pid, &status, 0);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}
