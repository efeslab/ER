/*
 * First KLEE tutorial: testing a small function
 */

#include <klee/klee.h>

int get_sign(int x) {
  if (x == 0)
     return 0;

  if (x < 0)
     return -1;
  else
     return 1;
}

int main() {
#ifdef KLEE_SYMBOLIC
  int a;
  klee_make_symbolic(&a, sizeof(a), "a");
#else
  int a = 2
#endif
  return get_sign(a);
}
