#pragma once

#include "NativeHookApi.h"

namespace legacy::sekai {
void setHookApi(InstallHook install);
void configure(bool allowed);
void libraryLoaded(const char *name, void *handle);
} // namespace legacy::sekai
