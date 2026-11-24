//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_hash_table.cpp
//
// Identification: src/container/hash/extendible_hash_table.cpp
//
// Copyright (c) 2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cassert>
#include <cstdlib>
#include <functional>
#include <list>
#include <utility>

#include "container/hash/extendible_hash_table.h"
#include "storage/page/page.h"

namespace bustub {

template <typename K, typename V>
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size) : bucket_size_(bucket_size) {
  BUSTUB_ENSURE(bucket_size > 0, "Bucket capacity must be positive");
  dir_.push_back(std::make_shared<Bucket>(bucket_size_));
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) -> size_t {
  size_t mask = (size_t{1} << global_depth_) - 1;
  return std::hash<K>()(key) & mask;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetGlobalDepthInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepthInternal() const -> int {
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int dir_index) const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetLocalDepthInternal(dir_index);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepthInternal(int dir_index) const -> int {
  return dir_[dir_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return GetNumBucketsInternal();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBucketsInternal() const -> int {
  return num_buckets_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Find(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  return dir_[IndexOf(key)]->Remove(key);
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) {
  std::scoped_lock<std::mutex> lock(latch_);
  while (!dir_[IndexOf(key)]->Insert(key, value)) {
    // Keep a copy: growing the directory invalidates references to its entries.
    auto bucket = dir_[IndexOf(key)];
    RedistributeBucket(bucket);
  }
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::RedistributeBucket(const std::shared_ptr<Bucket> &bucket) {
  const size_t split_bit = size_t{1} << bucket->GetDepth();
  if (bucket->GetDepth() == global_depth_) {
    const size_t old_size = dir_.size();
    dir_.resize(old_size * 2);
    for (size_t i = 0; i < old_size; ++i) {
      dir_[i + old_size] = dir_[i];
    }
    ++global_depth_;
  }

  bucket->IncrementDepth();
  auto new_bucket = std::make_shared<Bucket>(bucket_size_, bucket->GetDepth());
  ++num_buckets_;
  for (size_t i = 0; i < dir_.size(); ++i) {
    if (dir_[i] == bucket && (i & split_bit) != 0) {
      dir_[i] = new_bucket;
    }
  }

  auto &items = bucket->GetItems();
  auto &new_items = new_bucket->GetItems();
  for (auto it = items.begin(); it != items.end();) {
    if ((std::hash<K>()(it->first) & split_bit) != 0) {
      auto moved = it++;
      new_items.splice(new_items.end(), items, moved);
    } else {
      ++it;
    }
  }
}

//===--------------------------------------------------------------------===//
// Bucket
//===--------------------------------------------------------------------===//
template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t array_size, int depth) : size_(array_size), depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value) -> bool {
  for (const auto &item : list_) {
    if (item.first == key) {
      value = item.second;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key) -> bool {
  for (auto it = list_.begin(); it != list_.end(); ++it) {
    if (it->first == key) {
      list_.erase(it);
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value) -> bool {
  for (auto &item : list_) {
    if (item.first == key) {
      item.second = value;
      return true;
    }
  }
  if (IsFull()) {
    return false;
  }
  list_.emplace_back(key, value);
  return true;
}

template class ExtendibleHashTable<page_id_t, Page *>;
template class ExtendibleHashTable<Page *, std::list<Page *>::iterator>;
template class ExtendibleHashTable<int, int>;
// test purpose
template class ExtendibleHashTable<int, std::string>;
template class ExtendibleHashTable<int, std::list<int>::iterator>;

}  // namespace bustub
