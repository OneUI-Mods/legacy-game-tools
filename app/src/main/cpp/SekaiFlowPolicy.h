#pragma once

#include <cstdint>

namespace legacy::sekai::flow {
constexpr bool canSkipTitle(int state) { return state == 2 || state == 3; }
constexpr bool needsSecondClick(int before, int after) { return before == 2 && after == 3; }
constexpr bool isLiveFade(uintptr_t caller, uintptr_t base, float delay, float duration) {
  return (caller == base + 0x16b4854 || caller == base + 0x16b4cc4) && delay == 1.0f &&
         duration == 1.0f;
}
struct FadeTiming {
  float delay, duration;
};
constexpr FadeTiming liveTiming(bool enabled, uintptr_t caller, uintptr_t base, float delay,
                                float duration) {
  return {enabled && isLiveFade(caller, base, delay, duration) ? 0.0f : delay, duration};
}
} // namespace legacy::sekai::flow
