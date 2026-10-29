#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 64 // lmok-expect: LBC007

int BadName(int x) { // lmok-expect: LBC009
  register int i = x; // lmok-expect: LBC008
  char buf[16];
  gets(buf); // lmok-expect: LBC001
  strcat(buf, "x"); // lmok-expect: LBC001
  system("ls"); // lmok-expect: LBC004
  popen("id", "r"); // lmok-expect: LBC004
  if (x = 1) return 1; // lmok-expect: LBC005
  while (x = 2) x--; // lmok-expect: LBC005
  goto done; // lmok-expect: LBC003
  // x = gets(buf); // lmok-expect: LBC010
  // FIXME: no error handling // lmok-expect: LBC002
  done: return x;
}

int oldkr(a) // lmok-expect: LBC006
int a;
{ return a; }

implicit_ok(void) { return 0; }

void *alloc_cast(void) {
  char *p = (char *)malloc(8); // lmok-expect: LBC011
  return p;
}

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
