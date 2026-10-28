#pragma once

#include <cstddef>
#include <cstdint>
#include <link.h>
#include <string>

namespace legacy {
std::string buildId(const dl_phdr_info &info);
bool executableRange(const dl_phdr_info &info, uintptr_t offset, size_t size);
} // namespace legacy
