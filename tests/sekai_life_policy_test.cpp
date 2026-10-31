#include "../app/src/main/cpp/SekaiLifePolicy.h"
#include <cassert>

int main() {
  int object = 1, note = 2, method = 3;
  int calls = 0, life = 1000;
  auto original = [&](void *instance, void *receivedNote, const void *receivedMethod) {
    assert(instance == &object && receivedNote == &note && receivedMethod == &method);
    ++calls;
    life -= 80;
  };
  for (int i = 0; i < 100; ++i)
    legacy::sekai::life::dispatchDamage(true, original, &object, &note, &method);
  assert(calls == 0 && life == 1000);
  legacy::sekai::life::dispatchDamage(false, original, &object, &note, &method);
  assert(calls == 1 && life == 920);
  legacy::sekai::life::dispatchDamage(true, original, &object, &note, &method);
  assert(calls == 1 && life == 920);
}
