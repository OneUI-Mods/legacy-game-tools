#include "SekaiLifeHook.h"

#include "ElfIdentity.h"
#include "SekaiLifePolicy.h"
#include <android/log.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <mutex>
#include <string_view>

namespace legacy::sekai::life {
namespace {
constexpr char kTag[] = "LegacyGameTools";
constexpr char kBuildId[] = "cdd49c9a6324f45258da7e2fa21ac5d918950d4c";
constexpr uintptr_t kDamage = 0x1686048;
constexpr uint32_t kPrologue[] = {0xa9bc5ff8, 0xa90157f6, 0xa9024ff4, 0xa9037bfd};
using Damage = void (*)(void *instance, void *note, const void *method);
InstallHook installHook = nullptr;
Damage originalDamage = nullptr;
std::atomic<bool> enabled{false};
std::atomic<bool> installed{false};
std::atomic<bool> loggedSuppression{false};
std::mutex installMutex;

void damage(void *instance, void *note, const void *method) {
  const bool preventLoss = enabled.load(std::memory_order_relaxed);
  if (preventLoss && !loggedSuppression.exchange(true))
    __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_life_loss_prevented");
  dispatchDamage(preventLoss, originalDamage, instance, note, method);
}

int inspect(dl_phdr_info *info, size_t, void *) {
  if (!info->dlpi_name || !std::string_view(info->dlpi_name).ends_with("libil2cpp.so")) return 0;
  if (buildId(*info) != kBuildId || !executableRange(*info, kDamage, sizeof(kPrologue)) ||
      std::memcmp(reinterpret_cast<void *>(info->dlpi_addr + kDamage), kPrologue,
                  sizeof(kPrologue)) != 0) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_life_unsupported_binary");
    return 1;
  }
  if (installHook(reinterpret_cast<void *>(info->dlpi_addr + kDamage),
                  reinterpret_cast<void *>(damage), reinterpret_cast<void **>(&originalDamage)) != 0) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_life_hook_failed");
    return 1;
  }
  installed.store(true);
  __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_life_ready");
  return 1;
}

void tryInstall() {
  if (!enabled.load() || installed.load() || !installHook) return;
  std::lock_guard lock(installMutex);
  if (!installed.load()) dl_iterate_phdr(inspect, nullptr);
}
} // namespace

void setHookApi(InstallHook install) { installHook = install; }

void configure(bool value) {
  enabled.store(value);
  if (value) tryInstall();
}

void libraryLoaded(const char *name, void *) {
  if (name && std::string_view(name).ends_with("libil2cpp.so")) tryInstall();
}
} // namespace legacy::sekai::life
