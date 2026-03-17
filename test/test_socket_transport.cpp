#include "science/neural_pipeline/socket_transport.h"

#include <thread>
#include <unistd.h>

#include <gtest/gtest.h>

using science::neural_pipeline::Sample;
using science::neural_pipeline::SocketTransport;

static std::string unique_socket_path(const std::string &test_name) {
  return "/tmp/neural_pipeline_test_" + test_name + "_" +
         std::to_string(getpid()) + ".sock";
}

TEST(SocketTransportTest, ConnectAndSend) {
  auto path = unique_socket_path("connect");
  unlink(path.c_str());

  SocketTransport server({path});
  SocketTransport client({path});

  Sample sent;
  sent.timestamp_us = 123456;
  sent.channels = {1.0f, 2.0f, 3.0f, -4.5f};

  // Server binds and listens in a thread (blocks on accept)
  // Client connects after a brief delay to ensure server is listening
  std::thread server_thread([&]() {
    ASSERT_TRUE(server.listen_and_accept());
    ASSERT_TRUE(server.send(sent));
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ASSERT_TRUE(client.connect());
  Sample received;
  ASSERT_TRUE(client.receive(received));

  EXPECT_EQ(received.timestamp_us, sent.timestamp_us);
  ASSERT_EQ(received.channels.size(), sent.channels.size());
  for (size_t i = 0; i < sent.channels.size(); ++i) {
    EXPECT_FLOAT_EQ(received.channels[i], sent.channels[i]);
  }

  server_thread.join();
  server.close();
  client.close();
  unlink(path.c_str());
}

TEST(SocketTransportTest, MultiSampleStream) {
  auto path = unique_socket_path("multi");
  unlink(path.c_str());

  SocketTransport server({path});
  SocketTransport client({path});

  constexpr int kNumSamples = 100;
  constexpr int kNumChannels = 32;

  std::thread server_thread([&]() {
    ASSERT_TRUE(server.listen_and_accept());
    for (int i = 0; i < kNumSamples; ++i) {
      Sample s;
      s.timestamp_us = static_cast<uint64_t>(i) * 1000;
      s.channels.resize(kNumChannels);
      for (int c = 0; c < kNumChannels; ++c) {
        s.channels[c] = static_cast<float>(i * kNumChannels + c);
      }
      ASSERT_TRUE(server.send(s));
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ASSERT_TRUE(client.connect());
  for (int i = 0; i < kNumSamples; ++i) {
    Sample received;
    ASSERT_TRUE(client.receive(received));
    EXPECT_EQ(received.timestamp_us, static_cast<uint64_t>(i) * 1000);
    ASSERT_EQ(received.channels.size(), static_cast<size_t>(kNumChannels));
    for (int c = 0; c < kNumChannels; ++c) {
      EXPECT_FLOAT_EQ(received.channels[c],
                      static_cast<float>(i * kNumChannels + c));
    }
  }

  server_thread.join();
  server.close();
  client.close();
  unlink(path.c_str());
}
