//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k), frames_(num_frames) {
  BUSTUB_ENSURE(k > 0, "LRU-K requires a positive K");
}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t victim = -1;
  size_t oldest = std::numeric_limits<size_t>::max();
  bool victim_has_infinite_distance = false;
  for (size_t i = 0; i < frames_.size(); ++i) {
    const auto &frame = frames_[i];
    if (!frame.evictable_ || frame.timestamps_.empty()) {
      continue;
    }
    const bool infinite_distance = frame.timestamps_.size() < k_;
    const size_t timestamp = frame.timestamps_.front();
    if (victim == -1 || (infinite_distance && !victim_has_infinite_distance) ||
        (infinite_distance == victim_has_infinite_distance && timestamp < oldest)) {
      victim = static_cast<frame_id_t>(i);
      oldest = timestamp;
      victim_has_infinite_distance = infinite_distance;
    }
  }
  if (victim == -1) {
    return false;
  }
  frames_[victim].timestamps_.clear();
  frames_[victim].evictable_ = false;
  --curr_size_;
  *frame_id = victim;
  return true;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);
  BUSTUB_ENSURE(frame_id >= 0 && static_cast<size_t>(frame_id) < replacer_size_, "Invalid frame id");
  auto &history = frames_[frame_id].timestamps_;
  history.push_back(current_timestamp_++);
  if (history.size() > k_) {
    history.pop_front();
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock<std::mutex> lock(latch_);
  BUSTUB_ENSURE(frame_id >= 0 && static_cast<size_t>(frame_id) < replacer_size_, "Invalid frame id");
  auto &frame = frames_[frame_id];
  if (frame.timestamps_.empty() || frame.evictable_ == set_evictable) {
    return;
  }
  frame.evictable_ = set_evictable;
  if (set_evictable) {
    ++curr_size_;
  } else {
    --curr_size_;
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);
  BUSTUB_ENSURE(frame_id >= 0 && static_cast<size_t>(frame_id) < replacer_size_, "Invalid frame id");
  auto &frame = frames_[frame_id];
  if (frame.timestamps_.empty()) {
    return;
  }
  BUSTUB_ENSURE(frame.evictable_, "Cannot remove a non-evictable frame");
  frame.timestamps_.clear();
  frame.evictable_ = false;
  --curr_size_;
}

auto LRUKReplacer::Size() -> size_t {
  std::scoped_lock<std::mutex> lock(latch_);
  return curr_size_;
}

}  // namespace bustub
