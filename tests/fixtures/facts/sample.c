#include <stdio.h>
#include "util.h"
#define MAX 10
#define SQ(x) ((x)*(x))
typedef struct point { int x, y; char *name; int arr[4]; } point_t;
union u { int i; float f; };
enum color { RED, GREEN = 2 };
static int counter = 0;
char *names[3];
int *ptr;
char **argv2;
static int helper(int a, char *b);
int main(int argc, char **argv) {
  int local = SQ(2);
  point_t p;
  p.x = helper(local, argv[0]);
again:
  if (counter++ < MAX) goto again;
  return p.x;
}
static int helper(int a, char *b) { return a; }
int (*fp)(int);
char *get_name(void) { return 0; }
