#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 64

int BadName(int x) {
  register int i = x;
  char buf[16];
  gets(buf);
  strcat(buf, "x");
  system("ls");
  popen("id", "r");
  if (x = 1) return 1;
  while (x = 2) x--;
  goto done;
  // x = gets(buf);
  // FIXME: no error handling
  done: return x;
}

int oldkr(a)
int a;
{ return a; }

implicit_ok(void) { return 0; }
