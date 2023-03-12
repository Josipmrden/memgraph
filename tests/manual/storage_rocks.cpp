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

#include <gflags/gflags.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include "rocksdb/utilities/optimistic_transaction_db.h"
#include "rocksdb/utilities/transaction.h"

#include "storage/rocks/storage.hpp"
#include "utils/file.hpp"
#include "utils/logging.hpp"

using namespace rocksdb;

void PutKey(DB *db, const WriteOptions &opt, const std::string &key, const std::string &value) {
  auto s = db->Put(opt, key, value);
  MG_ASSERT(s.ok());
}

void NoKey(DB *db, const ReadOptions &opt, const std::string &key) {
  std::string value;
  auto s = db->Get(opt, key, &value);
  MG_ASSERT(s.IsNotFound());
  SPDLOG_INFO("no {} key", key);
}

void YesKey(DB *db, const ReadOptions &opt, const std::string &key) {
  std::string value;
  auto s = db->Get(opt, key, &value);
  MG_ASSERT(s.ok());
  SPDLOG_INFO("{}: {}", key, value);
}

void PutKey(Transaction *txn, const std::string &key, const std::string &value) {
  auto s = txn->Put(key, value);
  MG_ASSERT(s.ok());
}

void NoKey(Transaction *txn, const ReadOptions &opt, const std::string &key) {
  std::string value;
  auto s = txn->Get(opt, key, &value);
  MG_ASSERT(s.IsNotFound());
  SPDLOG_INFO("no {} key", key);
}

void YesKey(Transaction *txn, const ReadOptions &opt, const std::string &key) {
  std::string value;
  auto s = txn->Get(opt, key, &value);
  MG_ASSERT(s.ok());
  SPDLOG_INFO("{}: {}", key, value);
}

void RocksColumnFamilyFunc(std::filesystem::path &storage) {
  Options options;
  options.create_if_missing = true;
  DB *db = nullptr;
  OptimisticTransactionOptions txn_options;
  txn_options.set_snapshot = true;
  OptimisticTransactionDB *txn_db;
  auto s = OptimisticTransactionDB::Open(options, storage, &txn_db);

  MG_ASSERT(s.ok());
  db = txn_db->GetBaseDB();

  ColumnFamilyHandle *cf;
  s = txn_db->CreateColumnFamily(ColumnFamilyOptions(), "new_cf", &cf);
  MG_ASSERT(s.ok());

  s = txn_db->DestroyColumnFamilyHandle(cf);
  MG_ASSERT(s.ok());

  delete txn_db;

  std::vector<ColumnFamilyDescriptor> column_families;
  column_families.push_back(ColumnFamilyDescriptor(kDefaultColumnFamilyName, ColumnFamilyOptions()));
  column_families.push_back(ColumnFamilyDescriptor("new_cf", ColumnFamilyOptions()));

  std::vector<ColumnFamilyHandle *> handles;

  s = OptimisticTransactionDB::Open(options, storage, column_families, &handles, &txn_db);
  MG_ASSERT(s.ok());

  s = txn_db->Put(WriteOptions(), handles[1], Slice("key"), Slice("value"));
  MG_ASSERT(s.ok());
  std::string value;
  s = txn_db->Get(ReadOptions(), handles[1], Slice("key"), &value);
  MG_ASSERT(s.ok());

  // atomic write
  WriteBatch batch;
  batch.Put(handles[0], Slice("key2"), Slice("value2"));
  batch.Put(handles[1], Slice("key3"), Slice("value3"));
  batch.Delete(handles[0], Slice("key"));
  s = txn_db->Write(WriteOptions(), &batch);
  MG_ASSERT(s.ok());

  // drop column family
  s = txn_db->DropColumnFamily(handles[1]);
  MG_ASSERT(s.ok());

  // close db
  for (auto handle : handles) {
    s = txn_db->DestroyColumnFamilyHandle(handle);
    MG_ASSERT(s.ok());
  }

  delete txn_db;
}

void RocksBasicFunc(std::filesystem::path &storage) {
  Options options;
  options.create_if_missing = true;
  DB *db = nullptr;
  OptimisticTransactionOptions txn_options;
  txn_options.set_snapshot = true;
  OptimisticTransactionDB *txn_db;
  auto s = OptimisticTransactionDB::Open(options, storage, &txn_db);
  MG_ASSERT(s.ok());
  db = txn_db->GetBaseDB();

  PutKey(db, WriteOptions(), "key1", "value1");
  YesKey(db, ReadOptions(), "key1");

  WriteOptions write_options;
  auto *txn = txn_db->BeginTransaction(write_options, txn_options);

  ReadOptions read_options;
  // The transaction won't commit if someone outside changed the same value
  // this transaction intends to change. In other words, guarantee that noone
  // else has written a key since the start of the transaction.
  // txn->SetSnapshot();
  read_options.snapshot = txn->GetSnapshot();

  PutKey(txn, "key2", "value2");
  YesKey(txn, read_options, "key2");

  s = txn->Commit();
  assert(s.ok());
  delete txn;
  // Clear snapshot from read options since it is no longer valid
  read_options.snapshot = nullptr;

  YesKey(db, ReadOptions(), "key1");
  YesKey(db, ReadOptions(), "key2");

  delete txn_db;

  // memgraph::storage::rocks::Storage mgrocks;
  // mgrocks.Access();
}

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  spdlog::set_level(spdlog::level::info);

  std::filesystem::path storage = "rocks_experiment";
  if (!memgraph::utils::EnsureDir(storage)) {
    SPDLOG_ERROR("Unable to create storage folder on disk.");
    return 1;
  }

  RocksColumnFamilyFunc(storage);

  return 0;
}
