#include <stdio.h>

static int add_numbers(int left, int right) {
  int total = left + right;
  return total;
}

int main(void) {
  char name[32];
  if (fgets(name, sizeof name, stdin) == NULL) {
    return 1;
  }
  printf("hello %s\n", name);
  char *scratch = malloc(16);
  if (scratch == NULL) {
    return 1;
  }
  free(scratch);
  if (add_numbers(1, 2) == 3) {
    return 0;
  }
  return 1;
}
