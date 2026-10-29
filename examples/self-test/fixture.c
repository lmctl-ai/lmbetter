int positive(void) {
  return example(); // lmok-expect: EXAMPLE_CALL
}
int negative(void) {
  const char *text = "lmok-expect: EXAMPLE_CALL";
  return 0;
}
