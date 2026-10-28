#include "SekaiReadyHook.h"

#include "ElfIdentity.h"
#include "SekaiReadyPolicy.h"
#include <android/log.h>
#include <atomic>
#include <cstring>
#include <link.h>
#include <mutex>
#include <string_view>

namespace legacy::sekai::ready {
namespace {
constexpr char kBuildId[] = "cdd49c9a6324f45258da7e2fa21ac5d918950d4c";
constexpr uintptr_t kMove = 0x183a538, kFinish = 0x16b4c04, kDestroy = 0x16b4d54;
using Move = bool (*)(void *, const void *);
using Callback = void (*)(void *, const void *);
using NewWeak = uint32_t (*)(void *, bool);
using Target = void *(*)(uint32_t);
using Free = void (*)(uint32_t);
using ObjectClass = void *(*)(void *);
using ClassName = const char *(*)(void *);
using GameTime = float (*)(const void *);
using WriteBarrier = void (*)(void *, void **, void *);
Move originalMove = nullptr;
Callback originalFinish = nullptr, destroy = nullptr;
NewWeak newWeak = nullptr;
Target target = nullptr;
Free freeHandle = nullptr;
ObjectClass objectClass = nullptr;
ClassName className = nullptr;
GameTime gameTime = nullptr;
WriteBarrier writeBarrier = nullptr;
InstallHook installHook = nullptr;
RemoveHook removeHook = nullptr;
std::atomic<bool> enabled{false}, installed{false};
std::mutex installMutex;
// This single-player coroutine and Finish run synchronously on Unity's main thread.
bool captureFinish = false;
struct Session {
  uint32_t iterator = 0, transition = 0, fader = 0;
  bool waiting = false, observedFading = false;
  float deadline = 0;
} session;

template <typename T> T field(void *object, size_t offset) {
  T value;
  std::memcpy(&value, static_cast<char *>(object) + offset, sizeof(value));
  return value;
}
void reset() {
  for (uint32_t handle : {session.iterator, session.transition, session.fader}) {
    if (handle) {
      freeHandle(handle);
    }
  }
  session = {};
}
void finish(void *transition, const void *method) {
  originalFinish(transition, method);
  if (!captureFinish || !transition) {
    return;
  }
  void *fader = field<void *>(transition, 0x40);
  if (!fader) {
    return;
  }
  session.transition = newWeak(transition, false);
  session.fader = newWeak(fader, false);
  session.observedFading = field<int32_t>(fader, 0x7c) == 1;
}

bool move(void *iterator, const void *method) {
  const int32_t before = iterator ? field<int32_t>(iterator, 0x10) : -1;
  if (session.waiting && target(session.iterator) == iterator) {
    if (before != 4) {
      reset();
    } else {
      void *fader = target(session.fader);
      // Losing the weak target is not proof of completion. Fall back to the original deadline.
      const auto decision =
          decide(session.observedFading, !fader || field<int32_t>(fader, 0x7c) != 0, enabled.load(),
                 gameTime(nullptr), session.deadline);
      if (decision == Decision::Wait) {
        return true; // Current remains null: poll next frame.
      }
      if (decision == Decision::Ready) {
        if (void *transition = target(session.transition)) {
          destroy(transition, nullptr);
        }
        __android_log_print(ANDROID_LOG_INFO, "LegacyGameTools", "sekai_live_wait_completed_early");
      } else {
        __android_log_print(ANDROID_LOG_INFO, "LegacyGameTools",
                            "sekai_live_wait_original_timeout");
      }
      reset();
      return originalMove(iterator, method);
    }
  }
  void *controller = iterator && before == 3 ? field<void *>(iterator, 0x20) : nullptr;
  const bool candidate =
      installed.load() && enabled.load() && controller &&
      std::string_view(className(objectClass(controller))) == "SoloLiveController";
  if (!candidate) {
    return originalMove(iterator, method);
  }
  reset();
  captureFinish = true;
  const bool result = originalMove(iterator, method);
  captureFinish = false;
  // The native path has already awaited MusicReady, resumed audio, and started the exit fade.
  // Only exchange its six-second yield for polling; do not fabricate any asset/audio readiness.
  if (result && field<int32_t>(iterator, 0x10) == 4 && session.fader && session.observedFading) {
    session.iterator = newWeak(iterator, false);
    if (!session.iterator) {
      reset();
      return result;
    }
    session.deadline = gameTime(nullptr) + 6.0f;
    session.waiting = true;
    auto current = reinterpret_cast<void **>(static_cast<char *>(iterator) + 0x18);
    writeBarrier(iterator, current, nullptr);
    __android_log_print(ANDROID_LOG_INFO, "LegacyGameTools", "sekai_live_wait_polling");
  } else {
    reset();
  }
  return result;
}

struct Contract {
  uintptr_t offset;
  uint32_t words[4];
};
constexpr Contract kContracts[] = {
    {kMove, {0xf81d0ff5, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},
    {kFinish, {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},
    {kDestroy, {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},
};
int inspect(dl_phdr_info *info, size_t, void *) {
  if (!info->dlpi_name || !std::string_view(info->dlpi_name).ends_with("libil2cpp.so")) {
    return 0;
  }
  if (buildId(*info) != kBuildId) {
    return 1;
  }
  for (const auto &contract : kContracts) {
    if (!executableRange(*info, contract.offset, sizeof(contract.words)) ||
        std::memcmp(reinterpret_cast<void *>(info->dlpi_addr + contract.offset), contract.words,
                    sizeof(contract.words)) != 0) {
      __android_log_print(ANDROID_LOG_WARN, "LegacyGameTools", "sekai_ready_binary_mismatch");
      return 1;
    }
  }
  const uintptr_t base = info->dlpi_addr;
  newWeak = reinterpret_cast<NewWeak>(base + 0x12bc638);
  target = reinterpret_cast<Target>(base + 0x12bc640);
  freeHandle = reinterpret_cast<Free>(base + 0x12bc688);
  objectClass = reinterpret_cast<ObjectClass>(base + 0x12bc760);
  className = reinterpret_cast<ClassName>(base + 0x12bbe9c);
  gameTime = reinterpret_cast<GameTime>(base + 0x2386594);
  writeBarrier = reinterpret_cast<WriteBarrier>(base + 0x12bc670);
  destroy = reinterpret_cast<Callback>(base + kDestroy);
  void *finishTarget = reinterpret_cast<void *>(base + kFinish);
  if (installHook(finishTarget, reinterpret_cast<void *>(finish),
                  reinterpret_cast<void **>(&originalFinish)) != 0) {
    return 1;
  }
  if (installHook(reinterpret_cast<void *>(base + kMove), reinterpret_cast<void *>(move),
                  reinterpret_cast<void **>(&originalMove)) != 0) {
    removeHook(finishTarget);
    return 1;
  }
  installed.store(true);
  __android_log_print(ANDROID_LOG_INFO, "LegacyGameTools", "sekai_ready_wait_hook_ready");
  return 1;
}
void tryInstall() {
  if (!enabled.load() || installed.load() || !installHook) {
    return;
  }
  std::lock_guard lock(installMutex);
  if (!installed.load()) {
    dl_iterate_phdr(inspect, nullptr);
  }
}
} // namespace
void setHookApi(InstallHook install, RemoveHook remove) {
  installHook = install;
  removeHook = remove;
}
void configure(bool value) {
  enabled.store(value);
  tryInstall();
}
void libraryLoaded(const char *name, void *) {
  if (name && std::string_view(name).ends_with("libil2cpp.so")) {
    tryInstall();
  }
}
} // namespace legacy::sekai::ready
