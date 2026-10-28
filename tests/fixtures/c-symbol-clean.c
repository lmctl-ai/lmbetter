#include <stdio.h>

static int double_it(int x) {
  return x * 2;
}

int main(void) {
  printf("%d\n", double_it(21));
  return 0;
}
