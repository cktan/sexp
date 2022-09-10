#include "sexp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *readfile(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    perror("fopen");
    exit(1);
  }
  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET); /* same as rewind(f); */

  char *buf = malloc(fsize + 1);
  if (!buf) {
    perror("malloc");
    exit(1);
  }
  fread(buf, fsize, 1, fp);
  fclose(fp);
  buf[fsize] = 0;
  return buf;
}

int main(int argc, const char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s testfile\n", argv[0]);
    exit(1);
  }

  const char *s = readfile(argv[1]);
  const char *endp;
  sexp_object_t *ox = sexp_parse(s, strlen(s), &endp);
  if (!ox) {
    fprintf(stderr, "sexp_parse failed\n");
    exit(1);
  }
  const char *p = sexp_to_text(ox);
  if (!p) {
    fprintf(stderr, "sexp_to_text failed\n");
    exit(1);
  }
  printf("%s\n", p);

  if (*endp != 0) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Error: extra text after first expression in file\n");
    exit(1);
  }

  free((void *)p);
  free((void *)s);
  sexp_release(ox);
  return 0;
}
