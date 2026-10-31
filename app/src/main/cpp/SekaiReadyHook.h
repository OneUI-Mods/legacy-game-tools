#pragma once
#include "NativeHookApi.h"
namespace legacy::sekai::ready {
void setHookApi(InstallHook install, RemoveHook remove);
void configure(bool enabled);
void libraryLoaded(const char *name, void *handle);
} // namespace legacy::sekai::ready
