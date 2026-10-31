#include "../app/src/main/cpp/SekaiBpmPolicy.h"

#include <cassert>
#include <regex>
#include <string>

int main() {
  using namespace legacy::sekai;
  constexpr uintptr_t caller = 0x10000000 + 0x2fb734c;
  assert(shouldReplace(caller, caller, kOriginalPattern));
  assert(!shouldReplace(caller + 4, caller, kOriginalPattern));
  assert(!shouldReplace(caller, 0, kOriginalPattern));
  assert(!shouldReplace(caller, caller, u"^#BPM(?<bpm_id>\\d{2}):(?<bpm>.*)"));
  assert(!shouldReplace(caller, caller, u"^#BPM00:(?<bpm>.+)"));
  // A managed pattern with extra characters or an embedded NUL must not match.
  std::u16string extra(kOriginalPattern);
  extra.push_back(u'\0');
  assert(!shouldReplace(caller, caller, extra));

  // C++ ECMAScript regex has no named groups. Use the same character class
  // to exercise SUS definition IDs; native tests do not claim .NET execution.
  std::regex definition(R"(^#BPM([0-9A-Za-z]{2}):(.+))");
  std::smatch match;
  for (const char *id : {"00", "09", "0A", "0Z", "1A", "ZZ", "az"}) {
    std::string line = std::string("#BPM") + id + ":179.500";
    assert(std::regex_search(line, match, definition));
    assert(match[1] == id);
    assert(match[2] == "179.500");
  }
  for (const char *line : {"#BPM0:120", "#BPM000:120", "#BPM-1:120", "#BPMA_:120", "#BPM0A:"})
    assert(!std::regex_search(line, definition));
}
