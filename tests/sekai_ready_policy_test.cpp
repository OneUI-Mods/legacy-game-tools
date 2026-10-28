#include "../app/src/main/cpp/SekaiReadyPolicy.h"
#include <cassert>
int main() {
  using namespace legacy::sekai::ready;
  assert(decide(true, true, true, true, 1, 6) == Decision::Wait);
  assert(decide(true, false, true, true, 0.3f, 6) == Decision::Ready);
  assert(decide(false, false, true, true, 1, 6) == Decision::Wait);
  assert(decide(true, false, true, false, 1, 6) == Decision::Wait);
  assert(decide(true, true, true, true, 6, 6) == Decision::Timeout);
  assert(decide(true, false, false, true, 1, 6) == Decision::Wait);
}
