#include "ElfIdentity.h"

#include <cstring>
#include <elf.h>

namespace legacy {
size_t alignedNoteSize(size_t size) { return (size + 3) & ~size_t(3); }

std::string buildId(const dl_phdr_info &info) {
  constexpr char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < info.dlpi_phnum; ++i) {
    const auto &segment = info.dlpi_phdr[i];
    if (segment.p_type != PT_NOTE) {
      continue;
    }
    const auto *cursor = reinterpret_cast<const uint8_t *>(info.dlpi_addr + segment.p_vaddr);
    const auto *end = cursor + segment.p_memsz;
    while (static_cast<size_t>(end - cursor) >= sizeof(Elf64_Nhdr)) {
      Elf64_Nhdr note;
      std::memcpy(&note, cursor, sizeof(note));
      cursor += sizeof(note);
      const size_t nameSize = alignedNoteSize(note.n_namesz);
      const size_t descSize = alignedNoteSize(note.n_descsz);
      if (nameSize > static_cast<size_t>(end - cursor) ||
          descSize > static_cast<size_t>(end - cursor) - nameSize) {
        break;
      }
      if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
          std::memcmp(cursor, "GNU", 4) == 0) {
        std::string result;
        for (uint32_t n = 0; n < note.n_descsz; ++n) {
          uint8_t byte = cursor[nameSize + n];
          result += hex[byte >> 4];
          result += hex[byte & 15];
        }
        return result;
      }
      cursor += nameSize + descSize;
    }
  }
  return {};
}

bool executableRange(const dl_phdr_info &info, uintptr_t offset, size_t size) {
  for (size_t i = 0; i < info.dlpi_phnum; ++i) {
    const auto &segment = info.dlpi_phdr[i];
    if (segment.p_type == PT_LOAD && (segment.p_flags & PF_X) && offset >= segment.p_vaddr &&
        offset - segment.p_vaddr <= segment.p_memsz &&
        size <= segment.p_memsz - (offset - segment.p_vaddr)) {
      return true;
    }
  }
  return false;
}

} // namespace legacy
