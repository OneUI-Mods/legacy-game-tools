#pragma once

#include <cstdint>

namespace legacy {
using InstallHook = int (*)(void *, void *, void **);
using RemoveHook = int (*)(void *);
using LibraryLoaded = void (*)(const char *, void *);

struct NativeApi {
  uint32_t version;
  InstallHook hook;
  RemoveHook unhook;
};
} // namespace legacy
