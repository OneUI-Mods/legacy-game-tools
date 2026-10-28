#pragma once

#include "NativeHookApi.h"

namespace legacy::sekai::flow {
void setHookApi(InstallHook install, RemoveHook remove);
void configure(bool titleTap, bool fastTransition);
void libraryLoaded(const char *name, void *handle);
} // namespace legacy::sekai::flow
