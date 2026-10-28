#pragma once

#include <cstdint>
#include <string_view>

namespace legacy::sekai {
inline constexpr std::u16string_view kOriginalPattern = u"^#BPM(?<bpm_id>\\d{2}):(?<bpm>.+)";
inline constexpr char kBase36Pattern[] = "^#BPM(?<bpm_id>[0-9A-Za-z]{2}):(?<bpm>.+)";

inline bool shouldReplace(uintptr_t caller, uintptr_t expectedCaller, std::u16string_view pattern) {
  return expectedCaller != 0 && caller == expectedCaller && pattern == kOriginalPattern;
}
} // namespace legacy::sekai
