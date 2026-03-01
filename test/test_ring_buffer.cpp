#include "science/neural_pipeline/ring_buffer.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(RingBufferTest, BasicPushPop) {
  science::neural_pipeline::RingBuffer<int> buf(4);
  EXPECT_TRUE(buf.push(5));
  int out = 0;
  EXPECT_TRUE(buf.pop(out));
  EXPECT_EQ(out, 5);
}

TEST(RingBufferTest, FillAndDrain) {
  science::neural_pipeline::RingBuffer<int> buf(4);
  int in[4] = {1, 2, 3, 4};
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(buf.push(in[i]));
  }
  EXPECT_FALSE(buf.push(5));
  int out[4];
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(buf.pop(out[i]));
  }
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(out[i], in[i]);
  }
}

TEST(RingBufferTest, EmptyPop) {
  science::neural_pipeline::RingBuffer<int> buf(4);
  int out;
  EXPECT_TRUE(buf.empty());
  EXPECT_FALSE(buf.pop(out));
}

TEST(RingBufferTest, WrapAround) {
  science::neural_pipeline::RingBuffer<int> buf(4);
  int in[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  for (int i = 0; i < 8; i++) {
    EXPECT_TRUE(buf.push(in[i]));
    int out;
    EXPECT_TRUE(buf.pop(out));
    EXPECT_EQ(in[i], out);
  }
}

TEST(RingBufferTest, ConcurrentAccess) {
  const int N = 100005;
  const int M = 1024;
  science::neural_pipeline::RingBuffer<int> buf(M);
  std::vector<int> out(N);
  std::thread producer([&]() {
    for (int i = 0; i < N; i++) {
      while (buf.full()) {
      }
      EXPECT_TRUE(buf.push(i));
    }
  });
  std::thread consumer([&]() {
    for (int i = 0; i < N; i++) {
      while (buf.empty()) {
      }
      EXPECT_TRUE(buf.pop(out[i]));
    }
  });
  producer.join();
  consumer.join();
  for (int i = 0; i < N; i++) {
    EXPECT_EQ(i, out[i]);
  }
}
