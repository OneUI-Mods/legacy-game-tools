#include "../app/src/main/cpp/OverflowTracker.h"
#include <cassert>

int main() {
  OverflowTracker tracker;
  int firstOwner = 0, secondOwner = 0;
  // The 6th and later 4-GiB regions must remain independently searchable.
  for (uintptr_t region = 0x70; region < 0x90; ++region) {
    assert(tracker.insert(&firstOwner, (region << 32) | 0x1230));
  }
  assert(!tracker.contains(&secondOwner, (uintptr_t(0x75) << 32) | 0x1230));
  for (uintptr_t region = 0x70; region < 0x90; ++region) {
    uintptr_t address = (region << 32) | 0x1230;
    assert(tracker.contains(&firstOwner, address));
    assert(tracker.contains(&firstOwner, address + 7));
    assert(!tracker.contains(&firstOwner, address + 16));
    assert(!tracker.insert(&firstOwner, address));
    assert(tracker.erase(&firstOwner, address));
    assert(!tracker.contains(&firstOwner, address));
    assert(!tracker.erase(&firstOwner, address));
    assert(tracker.insert(&secondOwner, address));
    assert(!tracker.contains(&firstOwner, address));
  }
}
