#include "../app/src/main/cpp/SekaiFlowPolicy.h"
#include <cassert>

int main() {
  using namespace legacy::sekai::flow;
  assert(!canSkipTitle(0) && !canSkipTitle(1));
  assert(canSkipTitle(2) && canSkipTitle(3));
  assert(needsSecondClick(2, 3));
  assert(!needsSecondClick(3, 3) && !needsSecondClick(2, 2));
  constexpr uintptr_t base = 0x7000000000;
  assert(isLiveFade(base + 0x16b4854, base, 1, 1));
  assert(isLiveFade(base + 0x16b4cc4, base, 1, 1));
  assert(!isLiveFade(base + 0x16b4850, base, 1, 1));
  assert(!isLiveFade(base + 0x16b4854, base, 0, 1));
  assert(!isLiveFade(base + 0x16b4854, base, 1, 6));
}
