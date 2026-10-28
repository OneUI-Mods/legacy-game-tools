#pragma once

#include "NativeHookApi.h"

namespace legacy::sekai::life {
void setHookApi(InstallHook install);
void configure(bool enabled);
void libraryLoaded(const char *name, void *handle);
} // namespace legacy::sekai::life
