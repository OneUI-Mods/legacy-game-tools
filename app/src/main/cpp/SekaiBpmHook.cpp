#include "SekaiBpmHook.h"

#include "ElfIdentity.h"
#include "SekaiBpmPolicy.h"
#include <android/log.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <link.h>
#include <mutex>

namespace legacy::sekai {
namespace {
constexpr char kTag[] = "LegacyGameTools";
constexpr char kBuildId[] = "cdd49c9a6324f45258da7e2fa21ac5d918950d4c";
constexpr uintptr_t kRegexCtor = 0x1f6ab34;
constexpr uintptr_t kLoadBpmInfo = 0x2fb72e8;
constexpr uintptr_t kCtorCall = 0x2fb7348;
constexpr uintptr_t kLengthExport = 0x12bc7f4;
constexpr uintptr_t kCharsExport = 0x12bc7f8;
constexpr uintptr_t kNewExport = 0x12bc7fc;
constexpr uint32_t kPrologue[] = {0xf81d0ff5, 0xa9014ff4, 0xa9027bfd, 0x910083fd};
constexpr uint32_t kCtorCallInstruction = 0x97becdfb;

// Exact IL2CPP ABI from this game's original metadata. Strings are accessed
// through IL2CPP's exports rather than assuming a managed object layout.
using RegexConstructor = void (*)(void *instance, void *pattern, const void *method);
using StringLength = int32_t (*)(void *string);
using StringChars = const char16_t *(*)(void *string);
using StringNew = void *(*)(const char *utf8);

InstallHook installHook = nullptr;
RegexConstructor originalConstructor = nullptr;
StringLength stringLength = nullptr;
StringChars stringChars = nullptr;
StringNew stringNew = nullptr;
uintptr_t expectedCaller = 0;
std::atomic<bool> allowed{false};
std::atomic<bool> installed{false};
std::atomic<bool> loggedReplacement{false};
std::mutex installMutex;

[[gnu::noinline]] void regexConstructor(void *instance, void *pattern, const void *method) {
  // The return PC must be the single verified BL in LoadBpmInfo. Even another
  // caller using the identical regex is forwarded unchanged.
  uintptr_t caller = reinterpret_cast<uintptr_t>(
      __builtin_extract_return_addr(__builtin_return_address(0)));
  if (allowed.load(std::memory_order_relaxed) && caller == expectedCaller && pattern) {
    int32_t length = stringLength(pattern);
    if (length == static_cast<int32_t>(kOriginalPattern.size())) {
      const char16_t *chars = stringChars(pattern);
      if (chars && shouldReplace(caller, expectedCaller, {chars, static_cast<size_t>(length)})) {
        // Create a normal managed argument on the already attached calling
        // thread. Do not mutate/intern the game's shared pattern or cache an
        // unrooted managed pointer across calls.
        if (void *replacement = stringNew(kBase36Pattern)) {
          pattern = replacement;
          if (!loggedReplacement.exchange(true))
            __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_bpm_base36_applied");
        }
      }
    }
  }
  originalConstructor(instance, pattern, method);
}

bool matches(const dl_phdr_info &info, uintptr_t offset, const void *words, size_t size) {
  return executableRange(info, offset, size) &&
         std::memcmp(reinterpret_cast<void *>(info.dlpi_addr + offset), words, size) == 0;
}

struct Candidate {
  void *handle;
};

int inspect(dl_phdr_info *info, size_t, void *argument) {
  if (!info->dlpi_name || !std::string_view(info->dlpi_name).ends_with("libil2cpp.so")) return 0;
  if (buildId(*info) != kBuildId ||
      !matches(*info, kRegexCtor, kPrologue, sizeof(kPrologue)) ||
      !matches(*info, kLoadBpmInfo, kPrologue, sizeof(kPrologue)) ||
      !matches(*info, kCtorCall, &kCtorCallInstruction, sizeof(kCtorCallInstruction))) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_bpm_unsupported_binary");
    return 1;
  }
  void *handle = static_cast<Candidate *>(argument)->handle;
  auto length = reinterpret_cast<StringLength>(dlsym(handle, "il2cpp_string_length"));
  auto chars = reinterpret_cast<StringChars>(dlsym(handle, "il2cpp_string_chars"));
  auto create = reinterpret_cast<StringNew>(dlsym(handle, "il2cpp_string_new"));
  if (reinterpret_cast<uintptr_t>(length) != info->dlpi_addr + kLengthExport ||
      reinterpret_cast<uintptr_t>(chars) != info->dlpi_addr + kCharsExport ||
      reinterpret_cast<uintptr_t>(create) != info->dlpi_addr + kNewExport ||
      !executableRange(*info, kLengthExport, 12)) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_bpm_string_api_mismatch");
    return 1;
  }
  stringLength = length;
  stringChars = chars;
  stringNew = create;
  expectedCaller = info->dlpi_addr + kCtorCall + sizeof(uint32_t);
  void *target = reinterpret_cast<void *>(info->dlpi_addr + kRegexCtor);
  if (installHook(target, reinterpret_cast<void *>(regexConstructor),
                  reinterpret_cast<void **>(&originalConstructor)) != 0) {
    __android_log_print(ANDROID_LOG_WARN, kTag, "sekai_bpm_hook_failed");
    return 1;
  }
  installed.store(true);
  __android_log_print(ANDROID_LOG_INFO, kTag, "sekai_bpm_base36_ready");
  return 1;
}

void tryInstall(void *handle) {
  if (!allowed.load() || installed.load() || !installHook || !handle) return;
  std::lock_guard lock(installMutex);
  if (installed.load()) return;
  Candidate candidate{handle};
  dl_iterate_phdr(inspect, &candidate);
}
} // namespace

void setHookApi(InstallHook install) { installHook = install; }

void configure(bool value) {
  allowed.store(value);
  if (!value) return;
  // Handles both normal load notification and an engine already loaded before
  // Application.attach. RTLD_NOLOAD never loads a new game library.
  if (void *handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD)) {
    tryInstall(handle);
    dlclose(handle);
  }
}

void libraryLoaded(const char *name, void *handle) {
  if (name && std::string_view(name).ends_with("libil2cpp.so")) tryInstall(handle);
}
} // namespace legacy::sekai
