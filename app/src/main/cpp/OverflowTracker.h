#pragma once

#include <cstdint>
#include <unordered_set>

// A tracked allocation belongs to one Unity allocator, even if several
// allocators happen to use the same address at different times. This stores
// addresses only; ownership and the allocation/free operations remain entirely
// with the game.
struct AllocationKey {
  const void *owner;
  uintptr_t address;
  bool operator==(const AllocationKey &) const = default;
};

struct AllocationHash {
  size_t operator()(const AllocationKey &key) const {
    return std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(key.owner)) ^
           (std::hash<uintptr_t>{}(key.address) << 1);
  }
};

class OverflowTracker {
public:
  bool contains(void *owner, uintptr_t address) const {
    return entries_.contains({owner, trackingAddress(address)});
  }
  bool insert(void *owner, uintptr_t address) {
    return entries_.insert({owner, trackingAddress(address)}).second;
  }
  bool erase(void *owner, uintptr_t address) {
    return entries_.erase({owner, trackingAddress(address)}) != 0;
  }

private:
  // Match the original bitset: one entry covers a 16-byte allocation unit.
  static uintptr_t trackingAddress(uintptr_t address) { return address & ~uintptr_t(15); }
  std::unordered_set<AllocationKey, AllocationHash> entries_;
};
