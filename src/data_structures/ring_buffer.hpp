#pragma once

#include <atomic>
#include <chrono>
#include <experimental/optional>
#include <mutex>
#include <thread>
#include <utility>

#include "glog/logging.h"

#include "threading/sync/spinlock.hpp"

/**
 * A thread-safe ring buffer. Multi-producer, multi-consumer. Producers get
 * blocked if the buffer is full. Consumers get returnd a nullopt.
 *
 * @tparam TElement - type of element the buffer tracks.
 */
template <typename TElement>
class RingBuffer {
 public:
  RingBuffer(int capacity) : capacity_(capacity) {
    buffer_ = new TElement[capacity_];
  }

  RingBuffer(const RingBuffer &) = delete;
  RingBuffer(RingBuffer &&) = delete;
  RingBuffer &operator=(const RingBuffer &) = delete;
  RingBuffer &operator=(RingBuffer &&) = delete;

  ~RingBuffer() {
    delete[] buffer_;
  }

  template <typename... TArgs>
  void emplace(TArgs &&... args) {
    while (true) {
      {
        std::lock_guard<SpinLock> guard(lock_);
        if (size_ < capacity_) {
          buffer_[write_pos_++] = TElement(std::forward<TArgs>(args)...);
          write_pos_ %= capacity_;
          size_++;
          return;
        }
      }

      // Log a warning approximately once per second if buffer is full.
      LOG_EVERY_N(WARNING, 4000) << "RingBuffer full: worker waiting";
      // Sleep time determined using tests/benchmark/ring_buffer.cpp
      std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
  }

  std::experimental::optional<TElement> pop() {
    std::lock_guard<SpinLock> guard(lock_);
    if (size_ == 0) return std::experimental::nullopt;
    size_--;
    std::experimental::optional<TElement> result(
        std::move(buffer_[read_pos_++]));
    read_pos_ %= capacity_;
    return result;
  }

 private:
  int capacity_;
  TElement *buffer_;
  SpinLock lock_;
  int read_pos_{0};
  int write_pos_{0};
  int size_{0};
};
