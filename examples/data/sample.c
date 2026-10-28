#include <stdio.h>

int main(void) {
  char name[32];
  gets(name);
  printf("hello %s\n", name);
  return 0;
}
