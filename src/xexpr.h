#ifndef XEXPR_H
#define XEXPR_H

typedef struct xex_list_t xex_list_t;
typedef struct xex_string_t xex_string_t;
typedef struct xex_object_t xex_object_t;

struct xex_object_t {
  int type; // 'L' or 'S'
};

struct xex_list_t {
  int type;
  xex_object_t **vec;
  int max, top;
};

struct xex_string_t {
  int type;
  char *ptr;
};

static inline xex_list_t *xex_to_list(xex_object_t *obj) {
  return (xex_list_t *)(obj->type == 'L' ? obj : 0);
}
static inline xex_string_t *xex_to_string(xex_object_t *obj) {
  return (xex_string_t *)(obj->type == 'S' ? obj : 0);
}

#define xex_to_object(x)                                                       \
  (xex_object_t *)(((x)->type == 'L' || (x)->type == 'S') ? (x) : 0)

// Create a list object
xex_list_t *xex_list_create();

// Append to a list
int xex_list_append_object(xex_list_t *list, xex_object_t *obj);
int xex_list_append_list(xex_list_t *list, xex_list_t *lx);
int xex_list_append_string(xex_list_t *list, const char *str);

// Returns #elements in the list
static inline int xex_list_length(xex_list_t *list) { return list->top; }

// Returns list[i]
static inline xex_object_t *xex_list_get(xex_list_t *list, int idx) {
  return (0 <= idx && idx < list->top) ? list->vec[idx] : 0;
}

// Returns the string at list[i], or NULL if it is not a string
static inline const char *xex_list_get_string(xex_list_t *list, int idx) {
  return (0 <= idx && idx < list->top) ? ((xex_string_t *)list->vec[idx])->ptr
                                       : 0;
}

// Create a string object
xex_string_t *xex_string_create(const char *str);

// Make a deep copy
xex_object_t *xex_dup(xex_object_t *obj);

// Compare two objects for equality
int xex_equal(xex_object_t *x, xex_object_t *y);

// Traverse
typedef int xex_callback_t(xex_list_t *parent, int idx);
int xex_prefix(xex_list_t *lst, xex_callback_t *cb);
int xex_postfix(xex_list_t *lst, xex_callback_t *cb);

// Free obj
void xex_release(xex_object_t *obj);

// Context of a parser error
typedef struct xex_parse_error_t xex_parse_error_t;
struct xex_parse_error_t {
  char errmsg[200];
  int linenum;
  int lineoff;
};

// Deserialize buf[] into xexpr objects
xex_object_t *xex_parse(const char *buf, int len, const char **endp,
                        xex_parse_error_t *err);

// Serialize obj
char *xex_to_text(xex_object_t *obj);

#endif /* XEXPR_H */
