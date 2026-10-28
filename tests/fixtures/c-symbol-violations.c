#include <stdio.h>

static int unused_helper(int x) {
  return x * 2;
}

int recurse_me(int n) {
  if (n <= 0) { return 0; }
  return recurse_me(n - 1);
}

int main(void) {
  printf("%d\n", recurse_me(3));
  missing_init();
  return 0;
}
