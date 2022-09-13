#define _POSIX_C_SOURCE 200809L
#include "xexpr.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// clang-format off
#define CHECK(x)  if (x); else return -1
#define CHECKP(x) if (x); else return NULL
#define CHECKBAIL(x) if (x); else goto bail
// clang-format on    

static void *(*xex_malloc)(size_t) = malloc;
static void (*xex_free)(void*) = free;

void xex_set_memutil(void *(*mymalloc)(size_t), void (*myfree)(void *)) {
  if (mymalloc) {
    xex_malloc = mymalloc;
  }
  if (myfree) {
    xex_free = myfree;
  }
}

#define ALIGN8(sz) (((sz) + 7) & ~7)

#define malloc(x) error - forbidden - use MALLOC instead
#define MALLOC(a) xex_malloc(a)

#define free(x) error - forbidden - use FREE instead
#define FREE(a) xex_free(a)

#define calloc(x, y) error - forbidden - use CALLOC instead
static void *CALLOC(size_t nmemb, size_t sz) {
  int nb = ALIGN8(sz) * nmemb;
  void *p = MALLOC(nb);
  if (p) {
    memset(p, 0, nb);
  }
  return p;
}

static void *STRDUP(const char *str) {
  int nb = strlen(str) + 1;
  void *p = MALLOC(nb);
  if (p) {
    strcpy(p, str);
  }
  return p;
}

// Accumulator
typedef struct acc_t acc_t;
struct acc_t {
  char* ptr;
  int top, max;
};


// add p[] to the accumulator
static int accput(acc_t* acc, const char* p, int len) {
  if (acc->top + len >= acc->max) {
    int newmax = acc->max * 1.5 + len + 1;
    newmax = (newmax < 16 ? 16 : newmax); // minimum 16
    char* newptr = MALLOC(newmax);
    if (!newptr) {
      return -1;
    }
    if (acc->ptr) {
      memcpy(newptr, acc->ptr, acc->max);
      FREE(acc->ptr);
    }
    acc->ptr = newptr;
    acc->max = newmax;
  }
  
  assert(acc->top + len < acc->max);
  memcpy(acc->ptr + acc->top, p, len);
  acc->top += len;
  return 0;
}


static int need_quote(const char *s) {
  int special = (*s == 0);
  for (; *s; s++) {
    if (special) {
      break;
    }
    int ch = *s;
    special |= !(isprint(ch));
    special |= isspace(ch);
    special |= (ch == '(');
    special |= (ch == ')');
    special |= (ch == '"');
  }
  return special;
}

static int quote(const char *s, acc_t* acc) {
  // add the first "
  CHECK(0 == accput(acc, "\"", 1));

  for (;;) {
    // if there is no quote, put the whole s[] into acc, and done with s[].
    char* p = strchr(s, '"');
    if (!p) {
      CHECK(0 == accput(acc, s, strlen(s)));
      break; 
    }

    // put s[] up to the quote into acc
    CHECK(0 == accput(acc, s, p - s + 1));
    // add an extra "
    CHECK(0 == accput(acc, "\"", 1));
    // continue to the next segment of s[]
    s = p + 1;
  }

  // add the final "
  CHECK(0 == accput(acc, "\"", 1));
  return 0;
}

static int to_text(xex_object_t* obj, acc_t* acc) {
  xex_string_t *str;
  xex_list_t *list;

  str = xex_to_string(obj);
  if (str) {
    return need_quote(str->ptr) ? quote(str->ptr, acc) : accput(acc, str->ptr, strlen(str->ptr));
  }

  list = xex_to_list(obj);
  if (list) {
    CHECK(0 == accput(acc, "(", 1));
    for (int i = 0, top = list->top; i < top; i++) {
      if (i > 0) {
	CHECK(0 == accput(acc, " ", 1));
      }
      CHECK(0 == to_text(list->vec[i], acc));
    }
    CHECK(0 == accput(acc, ")", 1));
  }

  return 0;
}


char *xex_to_text(xex_object_t *obj) {
  acc_t acc = {0};
  CHECKBAIL(0 == to_text(obj, &acc));
  CHECKBAIL(0 == accput(&acc, "", 1));
  return acc.ptr;

 bail:
  if (acc.ptr) FREE(acc.ptr);
  return 0;
}


xex_object_t* xex_dup(xex_object_t* obj) {

  xex_string_t* str = 0;
  xex_list_t* lst = 0;

  if (0 != (str = xex_to_string(obj))) {
    return (xex_object_t*) xex_string_create(str->ptr);
  }

  if (0 != (lst = xex_to_list(obj))) {
    xex_list_t* ret = xex_list_create();
    CHECKP(ret);

    for (int i = 0, top = lst->top; i < top; i++) {
      xex_object_t* ox = xex_list_get(lst, i);
      CHECKP(ox);
      CHECKP(0 == xex_list_append_object(ret, ox));
    }

    return (xex_object_t*) ret;
  }
  
  return 0;
}


int xex_equal(xex_object_t* x, xex_object_t* y) {
  xex_string_t* xstr = 0;
  xex_string_t* ystr = 0;
  xex_list_t* xlst = 0;
  xex_list_t* ylst = 0;

  if (0 != (xstr = xex_to_string(x))) {
    ystr = xex_to_string(y);
    CHECK(ystr);
    return 0 == strcmp(xstr->ptr, ystr->ptr);
  }

  if (0 != (xlst = xex_to_list(x))) {
    ylst = xex_to_list(x);
    CHECK(ylst);
    CHECK(xlst->top == ylst->top);
    for (int i = 0, top = xlst->top; i < top; i++) {
      CHECK(xex_equal(xlst->vec[i], ylst->vec[i]));
    }
    return 1;
  }

  return 0;
}

static int prefix(xex_list_t* parent, int idx, xex_callback_t* fn) {
  // parent first
  xex_object_t* me = xex_list_get(parent, idx);
  CHECK(me);
  CHECK(0 == fn(parent, idx));

  xex_list_t* lst = xex_to_list(me);
  if (lst) {
    for (int i = 0, top = lst->top; i < top; i++) {
      CHECK(prefix(lst, i, fn));
    }
  }
  return 0;
}

static int postfix(xex_list_t* parent, int idx, xex_callback_t* fn) {
  // kids first
  xex_object_t* me = xex_list_get(parent, idx);
  CHECK(me);

  xex_list_t* lst = xex_to_list(me);
  if (lst) {
    for (int i = 0, top = lst->top; i < top; i++) {
      CHECK(prefix(lst, i, fn));
    }
  }

  CHECK(0 == fn(parent, idx));
  return 0;
}

int xex_prefix(xex_list_t* lst, xex_callback_t* fn) {
  xex_list_t root;
  xex_object_t* vec[1];
  vec[0] = (xex_object_t*)lst;
  root.type = 'L';
  root.vec = vec;
  root.top = 1;
  root.max = 1;

  return prefix(&root, 0, fn);
}


int xex_postfix(xex_list_t* lst, xex_callback_t* fn) {
  xex_list_t root;
  xex_object_t* vec[1];
  vec[0] = (xex_object_t*)lst;
  root.type = 'L';
  root.vec = vec;
  root.top = 1;
  root.max = 1;

  return postfix(&root, 0, fn);
}


void xex_release(xex_object_t *obj) {
  xex_string_t *str;
  xex_list_t *list;

  list = xex_to_list(obj);
  if (list) {
    for (int i = 0; i < list->top; i++) {
      xex_release(list->vec[i]);
    }
    FREE(list->vec);
    FREE(list);
    return;
  }

  str = xex_to_string(obj);
  if (str) {
    FREE(str->ptr);
    FREE(str);
    return;
  }
}

xex_list_t *xex_list_create() {
  xex_list_t *list = CALLOC(1, sizeof(*list));
  if (!list) {
    return 0;
  }

  list->type = 'L';
  return list;
}

int xex_list_append_object(xex_list_t *list, xex_object_t *obj) {
  assert(list->type == 'L');
  if (list->top >= list->max) {
    int newmax = list->max * 1.5 + 4;
    xex_object_t **newvec = MALLOC(newmax * sizeof(*newvec));
    if (!newvec) {
      return -1;
    }
    if (list->vec) {
      memcpy(newvec, list->vec, list->max * sizeof(*newvec));
      FREE(list->vec);
    }
    list->vec = newvec;
    list->max = newmax;
  }
  assert(list->top < list->max);
  list->vec[list->top++] = obj;
  return 0;
}

int xex_list_append_list(xex_list_t* list, xex_list_t* lx) {
  assert(lx->type == 'L');
  return xex_list_append_object(list, (xex_object_t*) lx);
}

int xex_list_append_string(xex_list_t* list, const char* s) {
  xex_string_t* sx = xex_string_create(s);
  CHECK(sx);
  return xex_list_append_object(list, (xex_object_t*) sx);
}


xex_string_t* xex_string_create(const char* str) {
  xex_string_t* p = CALLOC(1, sizeof(*p));
  if (p) {
    p->type = 'S';
    p->ptr = STRDUP(str);
    if (!p->ptr) {
      FREE(p);
      p = 0;
    }
  }
  return p;
}



typedef struct token_t token_t;
struct token_t {
  char type;       // [' ', '(', ')', 's', 'e'] s: string, e: eof
  const char *str; // points into source
  int len;
};

static token_t token_make(char type, const char *p, int plen) {
  assert(strchr(" ()se", type));
  token_t t = {type, p, plen};
  return t;
}

typedef struct scanner_t scanner_t;
struct scanner_t {
  const char *ptr;
  const char *end;
  int putback;
  token_t token;
  char* errbuf;
  int errbuflen;
};

static void scan_init(scanner_t *sp, const char *p, int plen,
		      char* errbuf, int errbuflen);
static token_t *scan_next(scanner_t *sp);
static token_t *scan_match(scanner_t *sp, int type);
static token_t *scan_peek(scanner_t *sp);

typedef struct parser_t parser_t;
struct parser_t {
  scanner_t scanner;
  char errbuf[200];
};
static void parse_init(parser_t *pp, const char *buf, int len);
static xex_object_t *parse_next(parser_t *pp);
static xex_object_t *parse_list(parser_t *pp);
static xex_object_t *parse_string(parser_t *pp);

static void parse_init(parser_t *pp, const char *buf, int len) {
  scan_init(&pp->scanner, buf, len, pp->errbuf, sizeof(pp->errbuf));
  pp->errbuf[0] = 0;
}

static xex_object_t *parse_string(parser_t *pp) {
  token_t *tok = scan_match(&pp->scanner, 's');
  if (!tok) {
    return 0;
  }
  const char *s = tok->str;
  int len = tok->len;

  // if quoted, take out the first and last "
  int quoted = (*s == '"');
  if (quoted) {
    s++;
    len -= 2;
  }

  // make a copy of s, and add extra byte for NUL
  char *p = MALLOC(len + 1);
  if (!p) {
    return 0;
  }
  memcpy(p, s, len);
  p[len] = 0;

  if (quoted) {
    // unescape two double-quote
    char *next = p;
    char *curr = p;
    char *q = p + len;
    for (; next < q; next++) {
      char ch = *next;
      if (ch == '"' && next + 1 < q && next[1] == '"') {
        next++;
      }
      *curr++ = ch;
    }
    *curr = 0;
  }

  xex_string_t *ret = MALLOC(sizeof(*ret));
  if (!ret) {
    FREE(p);
    return 0;
  }

  ret->type = 'S';
  ret->ptr = p;
  return (xex_object_t *)ret;
}

static xex_object_t *parse_list(parser_t *pp) {
  xex_list_t *list = 0;
  xex_object_t *obj = 0;

  // parse ( [WS] [ item WS item WS item [WS] ] )
  scanner_t *sp = &pp->scanner;
  if (!scan_match(sp, '(')) {
    snprintf(pp->errbuf, sizeof(pp->errbuf), "internal error");
    goto bail;
  }
  list = xex_list_create();
  if (!list) {
    snprintf(pp->errbuf, sizeof(pp->errbuf), "out of memory");
    goto bail;
  }
  // skip white space after (
  while (scan_match(sp, ' ')) {
    ;
  }

  // is this an empty list ?
  if (!scan_match(sp, ')')) {
    // fill in content...
    for (;;) {
      obj = parse_next(pp);
      if (!obj) {
        goto bail;
      }
      if (xex_list_append_object(list, obj)) {
	snprintf(pp->errbuf,sizeof(pp->errbuf),"out of memory");
        goto bail;
      }
      obj = 0;

      int has_space = (0 != scan_match(sp, ' '));
      if (scan_match(sp, ')')) {
        break;
      }
      if (!has_space) {
	snprintf(pp->errbuf, sizeof(pp->errbuf), "syntax error: need a space separator between list items");
        goto bail;
      }
    }
  }

  return (xex_object_t *)list;

bail:
  if (obj) {
    xex_release(obj);
  }
  if (list) {
    xex_release((xex_object_t *)list);
  }
  return 0;
}

static xex_object_t *parse_next(parser_t *pp) {
again:
  scanner_t *sp = &pp->scanner;
  token_t *tok = scan_peek(sp);
  if (!tok) {
    return 0;
  }
  switch (tok->type) {
  case ' ':
    scan_next(sp);  // skip over spaces
    goto again;
  case 's':
    return parse_string(pp);
  case '(':
    return parse_list(pp);
  case 'e':
    return 0;
  default:
    return 0;
  }
}

xex_object_t *xex_parse(const char *buf, int len, const char **endp, xex_parse_error_t* err) {
  parser_t parser;
  parse_init(&parser, buf, len);
  xex_object_t *ox = parse_next(&parser);
  if (!ox) {
    snprintf(err->errmsg, sizeof(err->errmsg), "%s",
	     parser.errbuf[0] ? parser.errbuf : "unknown error");
    int linenum = 1;
    int lineoff = 0;
    for (const char* p = buf; p < parser.scanner.ptr && p < buf + len; p++) {
      if (*p == '\n') {
	linenum++;
	lineoff = 0;
      } else {
	lineoff++;
      }
    }
    err->linenum = linenum;
    err->lineoff = lineoff;
  } else {
    // skip all whitespace after parsed expression
    while (scan_match(&parser.scanner, ' ')) {
      ;
    }
  }

  *endp = parser.scanner.ptr;
  return ox;
}

static token_t *_scan_quoted(scanner_t *sp);
static token_t *_scan_unquoted(scanner_t *sp);
static token_t *_scan_whitespace(scanner_t *sp);
static token_t *_scan_comment(scanner_t *sp);

static void scan_init(scanner_t *sp, const char *p, int plen, char* errbuf, int errbuflen) {
  memset(sp, 0, sizeof(*sp));
  sp->ptr = p;
  sp->end = p + plen;
  sp->errbuf = errbuf;
  sp->errbuflen = errbuflen;
}

static token_t *scan_peek(scanner_t *sp) {
  token_t *ret = scan_next(sp);
  assert(!sp->putback);
  sp->putback = (ret != 0);
  return ret;
}

static token_t *scan_match(scanner_t *sp, int type) {
  token_t *ret = scan_next(sp);
  if (ret && ret->type != type) {
    // putback if no match
    assert(!sp->putback);
    sp->putback = 1;
    ret = 0;
  }
  return ret;
}

/*
static token_t* scan_match_any(scanner_t* sp, const char* types) {
  token_t* ret = scan_next(sp);
  if (ret) {
    if (!strchr(types, ret->type)) {
      scan_putback(sp);
      ret = 0;
    }
  }
  return ret;
}
*/

static token_t *scan_next(scanner_t *sp) {
  sp->errbuf[0] = 0;
  
  if (sp->putback) {
    sp->putback = 0;
    return &sp->token;
  }

  memset(&sp->token, 0, sizeof(sp->token));
  if (sp->ptr >= sp->end) {
    sp->token = token_make('e', 0, 0);
    return &sp->token;
  }

  switch (*sp->ptr) {
  case '"':
    return _scan_quoted(sp);
  case '(':
  case ')':
    sp->token = token_make(*sp->ptr++, 0, 0);
    return &sp->token;
  case ';':
    return _scan_comment(sp);
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return _scan_whitespace(sp);
  default:
    return _scan_unquoted(sp);
  }
}

static token_t *_scan_quoted(scanner_t *sp) {
  assert('"' == *sp->ptr);
  const char *p = sp->ptr + 1;
  const char *q = sp->end;
  for (; p < q; p++) {
    int ch = *p;
    if (ch != '"') {
      continue;
    }
    if (p + 1 < q && p[1] == '"') {
      // two double-quotes
      p++;
    } else {
      break;
    }
  }
  if (p >= q) {
    // unterminated quote
    snprintf(sp->errbuf, sp->errbuflen, "syntax error: unterminated double-quote");
    return 0;
  }
  assert(*p == '"');
  p++;
  sp->token = token_make('s', sp->ptr, p - sp->ptr);
  sp->ptr = p;
  return &sp->token;
}

static token_t *_scan_unquoted(scanner_t *sp) {
  const char *p = sp->ptr;
  const char *q = sp->end;
  for (; p < q; p++) {
    int ch = *p;
    if (strchr(" \r\n\t()", ch)) {
      break;
    }
  }
  sp->token = token_make('s', sp->ptr, p - sp->ptr);
  sp->ptr = p;
  return &sp->token;
}

static token_t *_scan_whitespace(scanner_t *sp) {
  assert(strchr(" \r\n\t", *sp->ptr));
  const char *p = sp->ptr;
  const char *q = sp->end;
  // skip to first non-whitespace
  while (p < q && strchr(" \r\n\t", *p)) {
    p++;
  }
  sp->token = token_make(' ', 0, 0);
  sp->ptr = p;
  return &sp->token;
}

// note: this function will return a whitespace token
static token_t *_scan_comment(scanner_t *sp) {
  assert(';' == *sp->ptr);
  const char *p = sp->ptr;
  const char *q = sp->end;
  // skip to the first '\n'
  while (p < q && *p != '\n') {
    p++;
  }
  // return a whitespace
  sp->token = token_make(' ', 0, 0);
  // point to char after '\n'
  sp->ptr = (p < q ? p + 1 : q);
  return &sp->token;
}
