#include <stdio.h>

int main(void) {
  char buf[8];
  gets(buf);
  printf("%s\n", buf);
  return 0;
}
