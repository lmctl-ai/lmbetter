#include <stdio.h>

void read_input(void) {
  char buf[16];
  int n;
  scanf("%s", buf);
  scanf("%15s", buf);
  fscanf(stdin, "%d", &n);
}
