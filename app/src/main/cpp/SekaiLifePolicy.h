#pragma once

namespace legacy::sekai::life {
// Keep the original ABI and every argument intact whenever the option is off.
// Only Damage is intercepted: judgement, combo, recovery and result flow remain native.
template <typename Function>
void dispatchDamage(bool preventLoss, Function original, void *instance, void *note,
                    const void *method) {
  if (!preventLoss) original(instance, note, method);
}
} // namespace legacy::sekai::life
