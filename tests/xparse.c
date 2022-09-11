#include "xexpr.h"
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
  xex_parse_error_t err;
  xex_object_t *ox = xex_parse(s, strlen(s), &endp, &err);
  if (!ox) {
    fprintf(stderr, "xex_parse failed\n");
    if (err.linenum) {
      fprintf(stderr, "    %s\n", err.errmsg);
      fprintf(stderr, "    near line %d character %d\n", err.linenum,
              err.lineoff + 1);
    }
    exit(1);
  }
  const char *p = xex_to_text(ox);
  if (!p) {
    fprintf(stderr, "xex_to_text failed\n");
    exit(1);
  }
  printf("%s\n", p);

  if (*endp != 0) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Error: extra text after first expression in file\n");
    exit(1);
  }

  xex_object_t *copy = xex_dup(ox);
  if (!xex_equal(copy, ox)) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Error: xex_equal fails\n");
    exit(1);
  }

  const char *pp = xex_to_text(copy);
  if (!(0 == strcmp(p, pp))) {
    fprintf(stderr, "\n");
    fprintf(stderr, "Error: copied object is different from source\n");
    exit(1);
  }

  free((void *)pp);
  free((void *)p);
  free((void *)s);
  xex_release(ox);
  xex_release(copy);
  return 0;
}
