#ifndef SEXP_H
#define SEXP_H

typedef struct sexp_list_t sexp_list_t;
typedef struct sexp_string_t sexp_string_t;
typedef struct sexp_object_t sexp_object_t;

struct sexp_object_t {
  int type; // 'L' or 'S'
};

struct sexp_list_t {
  int type;
  sexp_object_t **vec;
  int max, top;
};

struct sexp_string_t {
  int type;
  char *ptr;
};

static inline sexp_list_t *sexp_to_list(sexp_object_t *obj) {
  return (sexp_list_t *)(obj->type == 'L' ? obj : 0);
}
static inline sexp_string_t *sexp_to_string(sexp_object_t *obj) {
  return (sexp_string_t *)(obj->type == 'S' ? obj : 0);
}

#define sexp_to_object(x)                                                      \
  (sexp_object_t *)(((x)->type == 'L' || (x)->type == 'S') ? (x) : 0)

sexp_list_t *sexp_list_create();
int sexp_list_append_object(sexp_list_t *list, sexp_object_t *obj);
int sexp_list_append_list(sexp_list_t *list, sexp_list_t *lx);
int sexp_list_append_string(sexp_list_t *list, const char *str);
int sexp_list_length(sexp_list_t *list);
sexp_object_t *sexp_list_get(sexp_list_t *list, int idx);
const char *sexp_list_get_string(sexp_list_t *list, int idx);

sexp_string_t *sexp_string_create(const char *str);

void sexp_release(sexp_object_t *obj);

sexp_object_t *sexp_parse(const char *buf, int len, const char **endp);
char *sexp_to_text(sexp_object_t *obj);

#endif /* SEXP_H */
