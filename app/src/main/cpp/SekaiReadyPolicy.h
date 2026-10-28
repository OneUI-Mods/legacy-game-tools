#pragma once
namespace legacy::sekai::ready {
enum class Decision { Wait, Ready, Timeout };
constexpr Decision decide(bool observedFading, bool stillFading, bool transitionFinished,
                          bool enabled, float now, float deadline) {
  if (enabled && observedFading && !stillFading && transitionFinished) {
    return Decision::Ready;
  }
  return now >= deadline ? Decision::Timeout : Decision::Wait;
}
} // namespace legacy::sekai::ready
