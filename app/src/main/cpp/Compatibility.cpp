#include "OverflowTracker.h"
#include "ElfIdentity.h"
#include "NativeHookApi.h"
#include "SekaiBpmHook.h"
#include "SekaiLifeHook.h"

#include <android/log.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <jni.h>
#include <link.h>
#include <mutex>
#include <string>

namespace {
constexpr char kTag[] = "LegacyGameTools";
using legacy::InstallHook;
using legacy::RemoveHook;
using legacy::NativeApi;
using legacy::LibraryLoaded;
using legacy::buildId;
using legacy::executableRange;
using TrackerFunction = bool (*)(void *, uintptr_t);
InstallHook installHook = nullptr;
RemoveHook removeHook = nullptr;
TrackerFunction originalContains = nullptr, originalInsert = nullptr, originalErase = nullptr;
std::atomic<bool> enabled{false};
std::atomic<bool> installed{false};
std::mutex trackingMutex, statusMutex;
OverflowTracker overflow;
std::string status = "waiting";
std::string grpcExtraRoot;
std::string grpcCombinedRoots;
using OverrideRoots = void (*)(const char *);
OverrideRoots originalOverrideRoots = nullptr;
using GrpcCreateCredentials = void *(*)(const char *, const char *, const char *);
GrpcCreateCredentials originalGrpcCreateCredentials = nullptr;
std::atomic<bool> grpcHookInstalled{false};
std::mutex grpcHookMutex;

// Preserve the library's normal public roots and append only the configured
// local CA. Certificate and hostname verification remain active.
void overrideGrpcRoots(const char *roots) {
  std::string combined = roots ? roots : "";
  combined += "\n" + grpcExtraRoot + "\n";
  grpcCombinedRoots = combined;
  originalOverrideRoots(combined.c_str());
  __android_log_print(ANDROID_LOG_INFO, kTag, "D4U local gRPC CA added");
}

void *createGrpcCredentials(const char *roots, const char *cert, const char *key) {
  if (roots) {
    std::string combined = std::string(roots) + "\n" + grpcExtraRoot + "\n";
    return originalGrpcCreateCredentials(combined.c_str(), cert, key);
  }
  return originalGrpcCreateCredentials(grpcCombinedRoots.empty() ? nullptr : grpcCombinedRoots.c_str(), cert, key);
}

struct Profile {
  const char *buildId;
  uintptr_t contains, insert, erase;
};
// Exact ELF build IDs from the installed, unmodified games. Offsets are never
// guessed for another engine revision. No game binaries are bundled.
constexpr Profile kProfiles[] = {
    {"a2b17a2b7ba7fb959c18b4e2b3c193c0eb89568e", 0xc177b8, 0xc17884, 0xc17ac4},
    {"02b347fdce531565010cebf5d8b4740dd1bf8e44", 0x6abb98, 0x6abc64, 0x6abea4},
};

void report(const char *value) {
  {
    std::lock_guard lock(statusMutex);
    status = value;
  }
  __android_log_print(ANDROID_LOG_INFO, kTag, "%s", value);
}

// Unity's original tracker has five 4-GiB address buckets. Modern Scudo can
// scatter a small heap across more buckets; this is unrelated to actual RAM
// use. Preserve the original tracker for representable pointers. Only
// overflowing pointers enter our additional set, and leave it when Unity frees
// them.
bool fitsOriginalTracker(void *owner, uintptr_t address) {
  uint32_t prefix = address >> 32;
  if (prefix == 0) {
    return true;
  }
  auto *bytes = static_cast<unsigned char *>(owner);
  for (size_t slot = 0; slot < 5; ++slot) {
    uint32_t nativePrefix;
    uintptr_t table;
    std::memcpy(&nativePrefix, bytes + 0xb0 + slot * 16, sizeof(nativePrefix));
    std::memcpy(&table, bytes + 0xb8 + slot * 16, sizeof(table));
    if (nativePrefix == prefix || table == 0) {
      return true;
    }
  }
  return false;
}

bool contains(void *owner, uintptr_t address) {
  std::lock_guard lock(trackingMutex);
  return overflow.contains(owner, address) || originalContains(owner, address);
}

bool insert(void *owner, uintptr_t address) {
  std::lock_guard lock(trackingMutex);
  if (overflow.contains(owner, address)) {
    return true;
  }
  if (fitsOriginalTracker(owner, address)) {
    return originalInsert(owner, address);
  }
  overflow.insert(owner, address);
  return true;
}

bool erase(void *owner, uintptr_t address) {
  std::lock_guard lock(trackingMutex);
  if (overflow.erase(owner, address)) {
    return true;
  }
  return originalErase(owner, address);
}

int inspectLibrary(dl_phdr_info *info, size_t, void *) {
  if (!info->dlpi_name || !std::string(info->dlpi_name).ends_with("/libunity.so")) {
    return 0;
  }
  std::string id = buildId(*info);
  const Profile *match = nullptr;
  for (const auto &profile : kProfiles) {
    if (id == profile.buildId) {
      match = &profile;
    }
  }
  if (!match) {
    report("unsupported_engine");
    return 1;
  }
  const uintptr_t offsets[] = {match->contains, match->insert, match->erase};
  // Guard function prologues as well as ELF identity before modifying code.
  constexpr uint32_t signatures[][4] = {
      {0xd360fc28, 0x340002a8, 0xaa1f03ea, 0x9102c00b},
      {0xd10243ff, 0xa90563f9, 0xa9065bf7, 0xa90753f5},
      {0xd10243ff, 0xf9002bf8, 0xa9065bf7, 0xa90753f5},
  };
  void *targets[3];
  void *replacements[] = {reinterpret_cast<void *>(contains), reinterpret_cast<void *>(insert),
                          reinterpret_cast<void *>(erase)};
  void **backups[] = {reinterpret_cast<void **>(&originalContains),
                      reinterpret_cast<void **>(&originalInsert),
                      reinterpret_cast<void **>(&originalErase)};
  for (size_t i = 0; i < 3; ++i) {
    if (!executableRange(*info, offsets[i], sizeof(signatures[i]))) {
      report("signature_mismatch");
      return 1;
    }
    targets[i] = reinterpret_cast<void *>(info->dlpi_addr + offsets[i]);
    if (std::memcmp(targets[i], signatures[i], sizeof(signatures[i])) != 0) {
      report("signature_mismatch");
      return 1;
    }
  }
  size_t done = 0;
  for (; done < 3; ++done) {
    if (installHook(targets[done], replacements[done], backups[done]) != 0) {
      break;
    }
  }
  if (done != 3) {
    while (done > 0) {
      removeHook(targets[--done]);
    }
    report("hook_failed");
    return 1;
  }
  installed = true;
  report("active");
  return 1;
}

void libraryLoaded(const char *name, void *handle) {
  legacy::sekai::libraryLoaded(name, handle);
  legacy::sekai::life::libraryLoaded(name, handle);
  if (name && !grpcExtraRoot.empty() && !grpcHookInstalled && std::string(name).ends_with("libgrpc_csharp_ext.so")) {
    std::lock_guard lock(grpcHookMutex);
    if (grpcHookInstalled) return;
    void *credentialsTarget = dlsym(handle, "grpcsharp_ssl_credentials_create");
    if (credentialsTarget) {
      int rc = installHook(credentialsTarget, reinterpret_cast<void *>(createGrpcCredentials),
                           reinterpret_cast<void **>(&originalGrpcCreateCredentials));
      __android_log_print(ANDROID_LOG_INFO, kTag, "D4U credentials hook=%d", rc);
    }
    void *target = dlsym(handle, "grpcsharp_override_default_ssl_roots");
    if (!target || installHook(target, reinterpret_cast<void *>(overrideGrpcRoots),
                               reinterpret_cast<void **>(&originalOverrideRoots)) != 0) {
      __android_log_print(ANDROID_LOG_ERROR, kTag, "D4U gRPC CA hook failed");
    } else grpcHookInstalled = true;
  }
  if (!enabled || installed || !name || !std::string(name).ends_with("libunity.so")) {
    return;
  }
  dl_iterate_phdr(inspectLibrary, nullptr);
}
} // namespace

extern "C" [[gnu::visibility("default"), gnu::used]]
LibraryLoaded native_init(const NativeApi *api) {
  if (!api || (api->version != 1 && api->version != 2) || !api->hook || !api->unhook) {
    return nullptr;
  }
  installHook = api->hook;
  removeHook = api->unhook;
  legacy::sekai::setHookApi(api->hook);
  legacy::sekai::life::setHookApi(api->hook);
  report("ready");
  return libraryLoaded;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_ryuya_legacygametools_HookEntry_configure(JNIEnv *, jclass, jboolean value) {
  enabled = value;
  if (!enabled) {
    report("disabled");
  }
}

extern "C" JNIEXPORT void JNICALL
Java_dev_ryuya_legacygametools_HookEntry_configureGrpcCa(JNIEnv *env, jclass, jstring path) {
  const char *filename = env->GetStringUTFChars(path, nullptr);
  if (!filename) return;
  std::ifstream file(filename);
  std::string pem((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  env->ReleaseStringUTFChars(path, filename);
  if (pem.find("-----BEGIN CERTIFICATE-----") != std::string::npos && pem.size() < 65536) {
    grpcExtraRoot = std::move(pem);
    __android_log_print(ANDROID_LOG_INFO, kTag, "D4U local gRPC CA configured");
  }
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_ryuya_legacygametools_HookEntry_nativeStatus(JNIEnv *env, jclass) {
  std::lock_guard lock(statusMutex);
  return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_dev_ryuya_legacygametools_HookEntry_configureSekaiBpm(JNIEnv *, jclass, jboolean allowed) {
  legacy::sekai::configure(allowed);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_ryuya_legacygametools_HookEntry_configureSekaiLife(JNIEnv *, jclass, jboolean enabled) {
  legacy::sekai::life::configure(enabled);
}
