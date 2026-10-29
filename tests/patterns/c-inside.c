void f(void) {
  while (x) { free(p); }
  free(q);
  if (y) { while (z) free(r); }
}
