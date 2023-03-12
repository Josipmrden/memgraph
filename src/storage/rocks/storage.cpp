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

#include "storage/rocks/storage.hpp"
#include "kvstore/kvstore.hpp"

namespace memgraph::storage::rocks {

Storage::Storage() {
  Options options;
  options.create_if_missing = true;
  DB *db = nullptr;

  std::vector<ColumnFamilyDescriptor> column_families;
  column_families.emplace_back("vertices", ColumnFamilyOptions());
  column_families.emplace_back("src_dest_edges", ColumnFamilyOptions());
  column_families.emplace_back("dest_src_edges", ColumnFamilyOptions());

  std::vector<ColumnFamilyHandle *> namespace_handles;

  std::filesystem::path storage = "rocks_experiment";
  auto status = DB::Open(options, storage, column_families, &namespace_handles, &db);

  if (!status.ok()) {
    throw kvstore::KVStoreError("RocksDB couldn't be initialized inside " + storage.string() + " -- " +
                                std::string(status.ToString()));
  }

  this->db_.reset(db);
}

VertexAccessor Storage::Accessor::CreateVertex() { throw 1; }

}  // namespace memgraph::storage::rocks
