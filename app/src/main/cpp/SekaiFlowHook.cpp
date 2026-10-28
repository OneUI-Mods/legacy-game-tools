#include "SekaiFlowHook.h"

#include "ElfIdentity.h"
#include "SekaiFlowPolicy.h"
#include <android/log.h>
#include <atomic>
#include <cstring>
#include <link.h>
#include <mutex>
#include <string_view>

namespace legacy::sekai::flow {
namespace {
constexpr char kTag[] = "LegacyGameTools";
constexpr char kBuildId[] = "cdd49c9a6324f45258da7e2fa21ac5d918950d4c";
constexpr uintptr_t kClick = 0x1662238, kSchedule = 0x1661c48, kDelayedVoice = 0x1663308;
constexpr uintptr_t kEventUpdate = 0x1b2e15c;
constexpr uintptr_t kFade = 0x18279d4, kFinishCall = 0x1661b3c;
constexpr uintptr_t kPlayVoice = 0x14d2d48, kStopVoice = 0x14d2c60;
struct Color {
  float r, g, b, a;
};
using Callback = void (*)(void *, const void *);
using Schedule = void (*)(void *, float, void *, void *, const void *);
using Fade = void (*)(void *, Color, float, float, void *, const void *);
using PlayVoice = uint32_t (*)(void *, void *, float, void *, void *, const void *);
using StopVoice = void (*)(void *, uint32_t, bool, const void *);
Callback originalClick = nullptr, originalDelayedVoice = nullptr;
Callback originalEventUpdate = nullptr, originalFinishCall = nullptr;
using NewWeak = uint32_t (*)(void *, bool);
using HandleTarget = void *(*)(uint32_t);
using FreeHandle = void (*)(uint32_t);
NewWeak newWeak = nullptr;
HandleTarget handleTarget = nullptr;
FreeHandle freeHandle = nullptr;
uint32_t titleHandle = 0, soundHandle = 0, titlePlayback = 0;
struct Touch {
  int32_t finger;
  float position[2], rawPosition[2], deltaPosition[2], deltaTime;
  int32_t tapCount, phase, type;
  float pressure, maximumPressure, radius, radiusVariance, altitude, azimuth;
};
static_assert(sizeof(Touch) == 68);
using TouchCount = int32_t (*)(const void *);
using GetTouch = Touch (*)(int32_t, const void *);
using MouseDown = bool (*)(int32_t, const void *);
TouchCount touchCount = nullptr;
GetTouch getTouch = nullptr;
MouseDown mouseDown = nullptr;
Schedule originalSchedule = nullptr;
Fade originalFade = nullptr;
Callback finishCall = nullptr;
PlayVoice originalPlayVoice = nullptr;
StopVoice stopVoice = nullptr;
InstallHook installHook = nullptr;
RemoveHook removeHook = nullptr;
uintptr_t imageBase = 0;
std::atomic<bool> titleEnabled{false}, transitionEnabled{false}, installed{false};
// TitleController is a singleton; all of these callbacks run on Unity's main thread.
// This state stores no managed pointers and resets for every scheduled title call.
bool titleSkipped = false;
bool titleInputLogged = false;
bool titleInputArmed = false;
std::atomic<bool> focused{false};
std::mutex installMutex;

int state(void *controller) {
  int value;
  std::memcpy(&value, static_cast<char *>(controller) + 0x98, sizeof(value));
  return value;
}

void click(void *controller, const void *method) {
  const int before = controller ? state(controller) : -1;
  const bool skip = installed.load() && titleEnabled.load() && canSkipTitle(before);
  if (skip && !titleSkipped) {
    titleSkipped = true;
    // Complete only the title-voice gate. Authentication and asset readiness remain native.
    if (soundHandle) {
      if (void *sound = handleTarget(soundHandle)) {
        stopVoice(sound, titlePlayback, false, nullptr);
      }
    }
    finishCall(controller, nullptr);
    __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_title_tap_skipped");
  }
  originalClick(controller, method);
  // Native first tap ends the opening animation; use its normal second-stage action
  // immediately, so one physical tap advances. Native code disables the title button.
  if (skip && needsSecondClick(before, state(controller))) {
    originalClick(controller, method);
  }
}

void schedule(void *controller, float delay, void *onPlay, void *onFinish, const void *method) {
  if (titleHandle) {
    freeHandle(titleHandle);
  }
  if (soundHandle) {
    freeHandle(soundHandle);
  }
  soundHandle = 0;
  titleHandle = titleEnabled.load() ? newWeak(controller, false) : 0;
  titleSkipped = false;
  titleInputLogged = false;
  titleInputArmed = false;
  __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_title_scheduled");
  originalSchedule(controller, delay, onPlay, onFinish, method);
}

void finish(void *controller, const void *method) {
  if (titleHandle && handleTarget(titleHandle) == controller) {
    freeHandle(titleHandle);
    titleHandle = 0;
  }
  if (soundHandle) {
    freeHandle(soundHandle);
  }
  soundHandle = 0;
  originalFinishCall(controller, method);
}

void eventUpdate(void *eventSystem, const void *method) {
  originalEventUpdate(eventSystem, method);
  if (!installed.load() || !titleEnabled.load() || !focused.load() || !titleHandle ||
      titleSkipped) {
    return;
  }
  void *controller = handleTarget(titleHandle);
  if (!controller || !canSkipTitle(state(controller))) {
    return;
  }
  if (!titleInputLogged) {
    titleInputLogged = true;
    __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_title_input_ready state=%d",
                        state(controller));
  }
  bool pressed = mouseDown(0, nullptr);
  const int32_t count = touchCount(nullptr);
  if (!titleInputArmed) {
    titleInputArmed = !pressed && count == 0;
    return;
  }
  for (int32_t i = 0; !pressed && i < count && i < 10; ++i) {
    pressed = getTouch(i, nullptr).phase == 0;
  }
  if (pressed) {
    __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_title_touch_received count=%d", count);
    click(controller, nullptr);
  }
}

void delayedVoice(void *closure, const void *method) {
  // A title call scheduled before the tap must not start after login begins.
  if (installed.load() && titleEnabled.load() && titleSkipped) {
    return;
  }
  originalDelayedVoice(closure, method);
}

[[gnu::noinline]] uint32_t playVoice(void *sound, void *cue, float volume, void *onPlay,
                                     void *onFinish, const void *method) {
  const uintptr_t caller =
      reinterpret_cast<uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
  const uint32_t playback = originalPlayVoice(sound, cue, volume, onPlay, onFinish, method);
  if (installed.load() && titleEnabled.load() && titleHandle && !titleSkipped &&
      caller == imageBase + 0x16633cc) {
    if (soundHandle) {
      freeHandle(soundHandle);
    }
    soundHandle = newWeak(sound, false);
    titlePlayback = playback;
    __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_title_voice_captured id=%u", playback);
  }
  return playback;
}

[[gnu::noinline]] void fade(void *instance, Color target, float delay, float duration,
                            void *onFinish, const void *method) {
  const uintptr_t caller =
      reinterpret_cast<uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)));
  const auto timing =
      liveTiming(installed.load() && transitionEnabled.load(), caller, imageBase, delay, duration);
  if (timing.delay != delay) {
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "sekai_live_white_hold_removed delay=%.3f duration=%.3f", timing.delay,
                        timing.duration);
  }
  originalFade(instance, target, timing.delay, timing.duration, onFinish, method);
}

struct Contract {
  uintptr_t address;
  uint32_t words[4];
};
constexpr Contract kContracts[] = {
    {kEventUpdate, {0xa9bc5ff8, 0xa90157f6, 0xa9024ff4, 0xa9037bfd}},
    {kClick, {0xd10283ff, 0xfd0033e8, 0xa90757f6, 0xa9084ff4}},
    {kSchedule, {0xfc1c0fe8, 0xa90157f6, 0xa9024ff4, 0xa9037bfd}},
    {kDelayedVoice, {0xa9bd57f6, 0xa9014ff4, 0xa9027bfd, 0x910083fd}},
    {kFade, {0x6dba33ed, 0x6d012beb, 0x6d0223e9, 0xf9001bf5}},
    {kFinishCall, {0xa9be4ff4, 0xa9017bfd, 0x910043fd, 0xb0016e33}},
    {kPlayVoice, {0xfc1c0fe8, 0xf90007f7, 0xa90157f6, 0xa9024ff4}},
    {kStopVoice, {0xf9401800, 0xb4000080, 0x12000042, 0xaa1f03e3}},
};

bool supported(const dl_phdr_info &info) {
  if (buildId(info) != kBuildId) {
    return false;
  }
  for (const auto &contract : kContracts) {
    if (!executableRange(info, contract.address, sizeof(contract.words)) ||
        std::memcmp(reinterpret_cast<void *>(info.dlpi_addr + contract.address), contract.words,
                    sizeof(contract.words)) != 0) {
      return false;
    }
  }
  for (auto [offset, instruction] : {std::pair{uintptr_t{0x16b4850}, uint32_t{0x9405cc61}},
                                     std::pair{uintptr_t{0x16b4cc0}, uint32_t{0x9405cb45}}}) {
    if (!executableRange(info, offset, 4) ||
        std::memcmp(reinterpret_cast<void *>(info.dlpi_addr + offset), &instruction, 4) != 0) {
      return false;
    }
  }
  return true;
}

int inspect(dl_phdr_info *info, size_t, void *) {
  if (!info->dlpi_name || !std::string_view(info->dlpi_name).ends_with("libil2cpp.so")) {
    return 0;
  }
  if (!supported(*info)) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_flow_unsupported_binary");
    return 1;
  }
  imageBase = info->dlpi_addr;
  newWeak = reinterpret_cast<NewWeak>(imageBase + 0x12bc638);
  handleTarget = reinterpret_cast<HandleTarget>(imageBase + 0x12bc640);
  freeHandle = reinterpret_cast<FreeHandle>(imageBase + 0x12bc688);
  touchCount = reinterpret_cast<TouchCount>(imageBase + 0x2f99254);
  getTouch = reinterpret_cast<GetTouch>(imageBase + 0x2f98d8c);
  mouseDown = reinterpret_cast<MouseDown>(imageBase + 0x2f98d0c);
  finishCall = reinterpret_cast<Callback>(imageBase + kFinishCall);
  stopVoice = reinterpret_cast<StopVoice>(imageBase + kStopVoice);
  void *targets[] = {reinterpret_cast<void *>(imageBase + kClick),
                     reinterpret_cast<void *>(imageBase + kSchedule),
                     reinterpret_cast<void *>(imageBase + kDelayedVoice),
                     reinterpret_cast<void *>(imageBase + kFade),
                     reinterpret_cast<void *>(imageBase + kEventUpdate),
                     reinterpret_cast<void *>(imageBase + kFinishCall),
                     reinterpret_cast<void *>(imageBase + kPlayVoice)};
  void *replacements[] = {
      reinterpret_cast<void *>(click),        reinterpret_cast<void *>(schedule),
      reinterpret_cast<void *>(delayedVoice), reinterpret_cast<void *>(fade),
      reinterpret_cast<void *>(eventUpdate),  reinterpret_cast<void *>(finish),
      reinterpret_cast<void *>(playVoice)};
  void **originals[] = {reinterpret_cast<void **>(&originalClick),
                        reinterpret_cast<void **>(&originalSchedule),
                        reinterpret_cast<void **>(&originalDelayedVoice),
                        reinterpret_cast<void **>(&originalFade),
                        reinterpret_cast<void **>(&originalEventUpdate),
                        reinterpret_cast<void **>(&originalFinishCall),
                        reinterpret_cast<void **>(&originalPlayVoice)};
  size_t done = 0;
  for (; done < 7; ++done) {
    if (installHook(targets[done], replacements[done], originals[done]) != 0) {
      break;
    }
  }
  if (done != 7) {
    while (done > 0) {
      removeHook(targets[--done]);
    }
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_flow_hook_failed");
    return 1;
  }
  installed.store(true);
  __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_flow_ready");
  return 1;
}

void tryInstall() {
  if ((!titleEnabled.load() && !transitionEnabled.load()) || installed.load() || !installHook) {
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
void setFocused(bool value) {
  focused.store(value);
  __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_game_focus=%d", value);
}

void configure(bool titleTap, bool fastTransition) {
  titleEnabled.store(titleTap);
  transitionEnabled.store(fastTransition);
  tryInstall();
}
void libraryLoaded(const char *name, void *) {
  if (name && std::string_view(name).ends_with("libil2cpp.so")) {
    tryInstall();
  }
}
} // namespace legacy::sekai::flow
