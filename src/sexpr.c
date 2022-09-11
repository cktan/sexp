#define _POSIX_C_SOURCE 200809L
#include "sexpr.h"
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
    char* newptr = realloc(acc->ptr, newmax);
    if (!newptr) {
      return -1;
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

static int to_text(sx_object_t* obj, acc_t* acc) {
  sx_string_t *str;
  sx_list_t *list;

  str = sx_to_string(obj);
  if (str) {
    return need_quote(str->ptr) ? quote(str->ptr, acc) : accput(acc, str->ptr, strlen(str->ptr));
  }

  list = sx_to_list(obj);
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


char *sx_to_text(sx_object_t *obj) {
  acc_t acc = {0};
  CHECKBAIL(0 == to_text(obj, &acc));
  CHECKBAIL(0 == accput(&acc, "", 1));
  return acc.ptr;

 bail:
  free(acc.ptr);
  return 0;
}

void sx_release(sx_object_t *obj) {
  sx_string_t *str;
  sx_list_t *list;

  list = sx_to_list(obj);
  if (list) {
    for (int i = 0; i < list->top; i++) {
      sx_release(list->vec[i]);
    }
    free(list->vec);
    free(list);
    return;
  }

  str = sx_to_string(obj);
  if (str) {
    free(str->ptr);
    free(str);
    return;
  }
}

sx_list_t *sx_list_create() {
  sx_list_t *list = calloc(1, sizeof(*list));
  if (!list) {
    return 0;
  }

  list->type = 'L';
  return list;
}

int sx_list_append_object(sx_list_t *list, sx_object_t *obj) {
  assert(list->type == 'L');
  if (list->top >= list->max) {
    int newmax = list->max * 1.5 + 4;
    sx_object_t **newvec = realloc(list->vec, sizeof(*list->vec) * newmax);
    if (!newvec) {
      return -1;
    }
    list->vec = newvec;
    list->max = newmax;
  }
  assert(list->top < list->max);
  list->vec[list->top++] = obj;
  return 0;
}


sx_string_t* sx_string_create(const char* str) {
  sx_string_t* p = calloc(1, sizeof(*p));
  if (p) {
    p->type = 'S';
    p->ptr = strdup(str);
    if (!p->ptr) {
      free(p);
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
};

static void scan_init(scanner_t *sp, const char *p, int plen);
static token_t *scan_next(scanner_t *sp);
static token_t *scan_match(scanner_t *sp, int type);
static token_t *scan_peek(scanner_t *sp);

typedef struct parser_t parser_t;
struct parser_t {
  scanner_t scanner;
  char errbuf[200];
};
static void parse_init(parser_t *pp, const char *buf, int len);
static sx_object_t *parse_next(parser_t *pp);
static sx_object_t *parse_list(parser_t *pp);
static sx_object_t *parse_string(parser_t *pp);

static void parse_init(parser_t *pp, const char *buf, int len) {
  scan_init(&pp->scanner, buf, len);
  pp->errbuf[0] = 0;
}

static sx_object_t *parse_string(parser_t *pp) {
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
  char *p = malloc(len + 1);
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

  sx_string_t *ret = malloc(sizeof(*ret));
  if (!ret) {
    free(p);
    return 0;
  }

  ret->type = 'S';
  ret->ptr = p;
  return (sx_object_t *)ret;
}

static sx_object_t *parse_list(parser_t *pp) {
  sx_list_t *list = 0;
  sx_object_t *obj = 0;

  // parse ( [WS] [ item WS item WS item [WS] ] )
  scanner_t *sp = &pp->scanner;
  if (!scan_match(sp, '(')) {
    goto bail;
  }
  list = sx_list_create();
  if (!list) {
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
      if (sx_list_append_object(list, obj)) {
        goto bail;
      }
      obj = 0;

      int has_space = (0 != scan_match(sp, ' '));
      if (scan_match(sp, ')')) {
        break;
      }
      if (!has_space) {
        goto bail;
      }
    }
  }

  return (sx_object_t *)list;

bail:
  if (obj) {
    sx_release(obj);
  }
  if (list) {
    sx_release((sx_object_t *)list);
  }
  return 0;
}

static sx_object_t *parse_next(parser_t *pp) {
again:
  scanner_t *sp = &pp->scanner;
  token_t *tok = scan_peek(sp);
  if (!tok) {
    return 0;
  }
  switch (tok->type) {
  case ' ':
    scan_next(sp);
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

sx_object_t *sx_parse(const char *buf, int len, const char **endp) {
  parser_t parser;
  parse_init(&parser, buf, len);
  sx_object_t *ox = parse_next(&parser);
  if (ox) {
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

static void scan_init(scanner_t *sp, const char *p, int plen) {
  memset(sp, 0, sizeof(*sp));
  sp->ptr = p;
  sp->end = p + plen;
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
