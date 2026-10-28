#pragma once
namespace legacy::sekai::ready {
enum class Decision { Wait, Ready, Timeout };
constexpr Decision decide(bool observedFading, bool stillFading, bool enabled, float now,
                          float deadline) {
  if (enabled && observedFading && !stillFading) {
    return Decision::Ready;
  }
  return now >= deadline ? Decision::Timeout : Decision::Wait;
}
} // namespace legacy::sekai::ready
