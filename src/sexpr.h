#ifndef SEXPR_H
#define SEXPR_H

typedef struct sx_list_t sx_list_t;
typedef struct sx_string_t sx_string_t;
typedef struct sx_object_t sx_object_t;

struct sx_object_t {
  int type; // 'L' or 'S'
};

struct sx_list_t {
  int type;
  sx_object_t **vec;
  int max, top;
};

struct sx_string_t {
  int type;
  char *ptr;
};

static inline sx_list_t *sx_to_list(sx_object_t *obj) {
  return (sx_list_t *)(obj->type == 'L' ? obj : 0);
}
static inline sx_string_t *sx_to_string(sx_object_t *obj) {
  return (sx_string_t *)(obj->type == 'S' ? obj : 0);
}

#define sx_to_object(x)                                                        \
  (sx_object_t *)(((x)->type == 'L' || (x)->type == 'S') ? (x) : 0)

sx_list_t *sx_list_create();
int sx_list_append_object(sx_list_t *list, sx_object_t *obj);
int sx_list_append_list(sx_list_t *list, sx_list_t *lx);
int sx_list_append_string(sx_list_t *list, const char *str);
int sx_list_length(sx_list_t *list);
sx_object_t *sx_list_get(sx_list_t *list, int idx);
const char *sx_list_get_string(sx_list_t *list, int idx);

sx_string_t *sx_string_create(const char *str);

void sx_release(sx_object_t *obj);

typedef struct sx_parse_error_t sx_parse_error_t;
struct sx_parse_error_t {
  char errmsg[200];
  int linenum;
  int lineoff;
};

sx_object_t *sx_parse(const char *buf, int len, const char **endp,
                      sx_parse_error_t *err);
char *sx_to_text(sx_object_t *obj);

#endif /* SEXPR_H */
