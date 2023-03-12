// Copyright 2023 Memgraph Ltd.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.txt; by using this file, you agree to be bound by the terms of the Business Source
// License, and you may not use this file except in compliance with the Business Source License.
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0, included in the file
// licenses/APL.txt.

#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include <fmt/format.h>

#include "slk/streams.hpp"
#include "utils/logging.hpp"

namespace memgraph::slk {

/// Class used for basic SLK use-cases.
/// It creates a `memgraph::slk::Builder` that can be written to. After you
/// have written the data to the builder, you can get a `memgraph::slk::Reader`
/// and try to decode the encoded data.
class Loopback {
 public:
  ~Loopback() {
    MG_ASSERT(builder_, "You haven't created a builder!");
    MG_ASSERT(reader_, "You haven't created a reader!");
    reader_->Finalize();
  }

  memgraph::slk::Builder *GetBuilder() {
    MG_ASSERT(!builder_, "You have already allocated a builder!");
    builder_ = std::make_unique<memgraph::slk::Builder>(
        [this](const uint8_t *data, size_t size, bool have_more) { Write(data, size, have_more); });
    return builder_.get();
  }

  memgraph::slk::Reader *GetReader() {
    MG_ASSERT(builder_, "You must first get a builder before getting a reader!");
    MG_ASSERT(!reader_, "You have already allocated a reader!");
    builder_->Finalize();
    auto ret = memgraph::slk::CheckStreamComplete(data_.data(), data_.size());
    MG_ASSERT(ret.status == memgraph::slk::StreamStatus::COMPLETE);
    MG_ASSERT(ret.stream_size == data_.size());
    size_ = ret.encoded_data_size;
    reader_ = std::make_unique<memgraph::slk::Reader>(data_.data(), data_.size());
    return reader_.get();
  }

  size_t size() { return size_; }

 private:
  void Write(const uint8_t *data, size_t size, bool have_more) {
    for (size_t i = 0; i < size; ++i) {
      data_.push_back(data[i]);
    }
  }

  std::vector<uint8_t> data_;
  std::unique_ptr<memgraph::slk::Builder> builder_;
  std::unique_ptr<memgraph::slk::Reader> reader_;
  size_t size_{0};
};

}  // namespace memgraph::slk