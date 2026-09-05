#include "../include/parser.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/assert.h"
#include "lilc/eq.h"
#include "lilc/hash.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/str.h"
#include "lilc/todo.h"
#include <lilc/bump.h>
#include <setjmp.h>
#include <stdio.h>

void p_parser_init(p_parser_t *parser) {
  bump_init(&parser->ast_bump, 16000);
  bump_allocator_init(&parser->ast_allocator, &parser->ast_bump);

  hashmap_init(&parser->typedef_names, &parser->ast_allocator, char *,
               p_ast_type_t, str_ptrv_hash, str_ptrv_eq, NULL);
}

void p_parser_deinit(p_parser_t *parser) { bump_free(&parser->ast_bump); }

static bool p_next_token(p_parser_t *parser) {
  if (parser->peek_tok->kind == P_TOKEN_EOF) {
    parser->cur_tok = parser->peek_tok;
    return false;
  }

  parser->cur_tok = parser->peek_tok;
  parser->peek_tok++;

  return true;
}

static char *p_cur_tok_fmt(p_parser_t *parser) {
  return p_tok_fmt(parser->cur_tok, &parser->ast_allocator).string;
}

static char *p_peek_tok_fmt(p_parser_t *parser) {
  return p_tok_fmt(parser->peek_tok, &parser->ast_allocator).string;
}

static inline bool p_expect_cur_tok(p_parser_t *parser, p_tok_kind_e tok_kind) {
  bool matches = parser->cur_tok->kind == tok_kind;
  if (matches) {
    p_next_token(parser);
  }
  return matches;
}

static inline bool p_expect_peek_tok(p_parser_t *parser,
                                     p_tok_kind_e tok_kind) {
  bool matches = parser->peek_tok->kind == tok_kind;
  if (matches) {
    p_next_token(parser);
  }
  return matches;
}

static bool p_check_cur_tok(p_parser_t *parser, p_tok_kind_e tok_kind) {
  return parser->cur_tok->kind == tok_kind;
}

static bool p_check_peek_tok(p_parser_t *parser, p_tok_kind_e tok_kind) {
  return parser->peek_tok->kind == tok_kind;
}

struct p_builtin_type {
  const char *name;
  p_type_kind_e type_kind;
  bool signable;
  p_type_kind_e signed_ty;
  p_type_kind_e unsigned_ty;
};

const struct p_builtin_type glob_builtin_types[] = {
    {"void", P_TY_VOID, false, 0, 0},
    {"bool", P_TY_BOOL, false, 0, 0},
    {"char", P_TY_CHAR, true, P_TY_SCHAR, P_TY_UCHAR},
    {"short", P_TY_SHORT, true, P_TY_SHORT, P_TY_USHORT},
    {"int", P_TY_INT, true, P_TY_INT, P_TY_UINT},
    {"long", P_TY_LONG, true, P_TY_LONG, P_TY_ULONG},
    {"signed", P_TY_INT, false, 0, 0},
    {"unsigned", P_TY_UINT, false, 0, 0},
    {"float", P_TY_FLOAT, false, 0, 0},
    {"double", P_TY_DOUBLE, false, 0, 0},
};

static i32 p_builtin_type_idx(const char *ident) {
  for (u64 i = 0;
       i < sizeof(glob_builtin_types) / sizeof(struct p_builtin_type); i++) {
    if (str_eq(ident, glob_builtin_types[i].name)) {
      return i;
    }
  }
  return -1;
}

struct p_storage_class {
  const char *name;
  p_ast_storage_class_e sc;
};

const struct p_storage_class glob_storage_classes[] = {
    {"typedef", P_SC_TYPEDEF},   {"extern", P_SC_EXTERN},
    {"static", P_SC_STATIC},     {"auto", P_SC_AUTO},
    {"register", P_SC_REGISTER},
};

static bool p_parse_decl_sc(p_tok_t token, p_ast_storage_class_e *sc) {
  if (token.kind != P_TOKEN_IDENT) {
    if (sc != NULL) {
      *sc = P_SC_NONE;
    }
    return false;
  }

  for (u64 i = 0;
       i < sizeof(glob_storage_classes) / sizeof(struct p_storage_class); i++) {
    if (str_eq(token.ident_literal, glob_storage_classes[i].name)) {
      if (sc != NULL) {
        *sc = glob_storage_classes[i].sc;
      }
      return true;
    }
  }

  if (sc != NULL) {
    *sc = P_SC_NONE;
  }

  return false;
}

struct p_type_qual {
  const char *name;
  p_ast_type_qual_e qual;
};

const struct p_type_qual glob_type_quals[] = {
    {"const", P_TQ_CONST},
    {"volatile", P_TQ_VOLATILE},
    {"restrict", P_TQ_RESTRICT},
    {"atomic", P_TQ_ATOMIC},
};

static bool p_parse_type_qual(p_tok_t token, u32 *quals) {
  if (token.kind != P_TOKEN_IDENT) {
    return false;
  }

  for (u64 i = 0; i < sizeof(glob_type_quals) / sizeof(struct p_type_qual);
       i++) {
    if (str_eq(token.ident_literal, glob_type_quals[i].name)) {
      if (quals != NULL) {
        *quals |= glob_type_quals[i].qual;
      }
      return true;
    }
  }

  return false;
}

static p_ast_type_t p_ident_to_type(const char *ident, i32 builtin_ty_idx) {
  p_ast_type_t type = {0};
  if (builtin_ty_idx != -1) {
    type.kind = glob_builtin_types[builtin_ty_idx].type_kind;
    return type;
  }

  type.kind = P_TY_TYPEDEF_NAME;
  type.u.typedef_name = ident;
  return type;
}

static bool p_is_type_ident(p_parser_t *parser, const char *ty_ident,
                            i32 *builtin_ty_idx) {
  i32 _builtin_ty_idx = p_builtin_type_idx(ty_ident);
  if (builtin_ty_idx != NULL) {
    *builtin_ty_idx = _builtin_ty_idx;
  }
  if (_builtin_ty_idx != -1) {
    return true;
  }

  return hashmap_contains(&parser->typedef_names, &ty_ident);
}

enum p_declarator_parse_context {
  P_TY_PARSE_CTX_FUNC_ARG,
  P_TY_PARSE_CTX_REC_FIELD,
  P_TY_PARSE_CTX_DECL,
};

static bool p_parse_init(p_parser_t *parser, p_ast_init_t *init);

static bool p_parse_declarators(p_parser_t *parser, p_ast_type_t *base_ty,
                                p_ast_declarator_t **declarators,
                                p_ast_arg_t *arg, p_ast_storage_class_e *sc,
                                enum p_declarator_parse_context parse_ctx);

// cur_tok is union/struct ident
static bool p_parse_rec_type(p_parser_t *parser, p_type_kind_e type_kind,
                             struct p_ast_type_rec *rec_type) {
  ASSERT(type_kind == P_TY_STRUCT || type_kind == P_TY_UNION,
         "Only union and struct are valid type kinds for record type parsing");

  const char *record_kind = type_kind == P_TY_STRUCT ? "struct" : "union";

  p_next_token(parser);

  bool has_tag = false;

  // TODO: Ensure we are not using keywords in cases like this
  if (p_expect_cur_tok(parser, P_TOKEN_IDENT)) {
    rec_type->tag = parser->cur_tok->ident_literal;
    has_tag = true;
  }

  rec_type->fields = array_new(p_ast_rec_field_t, &parser->ast_allocator);

  if (p_expect_cur_tok(parser, P_TOKEN_LCURLY)) {
    while (!p_check_cur_tok(parser, P_TOKEN_RCURLY)) {
      p_ast_rec_field_t field = {
          .decls = array_new(p_ast_declarator_t, &parser->ast_allocator)};
      // TODO: Fix these parameters
      bool field_success =
          p_parse_declarators(parser, &field.base_type, &field.decls, NULL,
                              NULL, P_TY_PARSE_CTX_REC_FIELD);
      if (!field_success) {
        log_error("Failed to parse type for %s field", record_kind);
        return false;
      }

      array_add(rec_type->fields, field);
    }
    rec_type->is_complete = true;
  } else if (has_tag) {
    rec_type->is_complete = false;
  } else {
    log_error("Struct needs tag or fields, found neither");
    return false;
  }

  return true;
}

// cur_tok is enum ident
static bool p_parse_enum_type(p_parser_t *parser,
                              struct p_ast_type_enum *enum_type) {
  p_next_token(parser);

  bool has_tag = false;

  // TODO: Ensure we are not using keywords in cases like this
  if (p_expect_cur_tok(parser, P_TOKEN_IDENT)) {
    enum_type->tag = parser->cur_tok->ident_literal;
    has_tag = true;
  }

  enum_type->items = array_new(p_ast_enum_item_t, &parser->ast_allocator);

  if (p_expect_cur_tok(parser, P_TOKEN_LCURLY)) {
    while (!p_check_cur_tok(parser, P_TOKEN_RCURLY)) {
      p_ast_enum_item_t item = {};
      if (!p_expect_cur_tok(parser, P_TOKEN_IDENT)) {
        log_error("Expected enum item name, received token: %s",
                  p_cur_tok_fmt(parser));
        return false;
      }

      item.name = parser->cur_tok->ident_literal;

      array_add(enum_type->items, item);

      if (!p_expect_cur_tok(parser, P_TOKEN_COMMA) &&
          !p_check_cur_tok(parser, P_TOKEN_RCURLY)) {

        log_error("Expected token comma or right curly bracket after enum item "
                  "%s, received token: %s",
                  item.name, p_cur_tok_fmt(parser));
        return false;
      }
    }
    enum_type->is_complete = true;
  } else if (has_tag) {
    enum_type->is_complete = false;
  } else {
    log_error("Enum needs tag or items, found neither");
    return false;
  }

  return true;
}

// Cur tok is first field or end_tok
static bool p_parse_args_list(p_parser_t *parser, p_ast_arg_t **args) {
  while (!p_check_cur_tok(parser, P_TOKEN_RPAREN)) {
    log_debug("Parsing args");
    p_ast_arg_t arg = {0};
    bool ty_success = p_parse_declarators(parser, &arg.ty, NULL, &arg, NULL,
                                          P_TY_PARSE_CTX_FUNC_ARG);
    if (!ty_success) {
      log_error("Failed to parse type in field list");
      return false;
    }

    if (arg.name == NULL && p_expect_peek_tok(parser, P_TOKEN_IDENT)) {
      arg.name = parser->cur_tok->ident_literal;
    }

    array_add(*args, arg);

    if (p_expect_cur_tok(parser, P_TOKEN_COMMA)) {
      if (p_check_cur_tok(parser, P_TOKEN_RPAREN)) {
        log_error("Trailing comma not allowed in args list");
        return false;
      }
      p_next_token(parser);
    } else if (!p_check_cur_tok(parser, P_TOKEN_RPAREN)) {
      log_error("Expected comma or right parenthesis after arg list entry, "
                "received token: %s",
                p_cur_tok_fmt(parser));
      return false;
    }
  }

  return true;
}

static bool p_parse_expr(p_parser_t *parser, p_ast_expr_t *expr);

static bool p_parse_base_type(p_parser_t *parser, p_ast_type_t *base_ty) {
  if (!p_check_cur_tok(parser, P_TOKEN_IDENT)) {
    log_error("Expected ident for base type parsing");
    return false;
  }

  i32 builtin_ty_idx = 0;
  char *ident = parser->cur_tok->ident_literal;

  if (str_eq(ident, "struct")) {
    base_ty->kind = P_TY_STRUCT;
    bool struct_ty_success =
        p_parse_rec_type(parser, P_TY_STRUCT, &base_ty->u.rec);
    if (!struct_ty_success) {
      log_error("Failed to parse struct type");
      return false;
    }
  } else if (str_eq(ident, "union")) {
    base_ty->kind = P_TY_UNION;
    bool union_ty_success =
        p_parse_rec_type(parser, P_TY_UNION, &base_ty->u.rec);
    if (!union_ty_success) {
      log_error("Failed to parse union type");
      return false;
    }
  } else if (str_eq(ident, "enum")) {
    base_ty->kind = P_TY_ENUM;
    bool en_ty_success = p_parse_enum_type(parser, &base_ty->u.en);
    if (!en_ty_success) {
      log_error("Failed to parse enum type");
      return false;
    }
  } else if (p_is_type_ident(parser, ident, &builtin_ty_idx)) {
    *base_ty = p_ident_to_type(ident, builtin_ty_idx);
  } else {
    log_error("Ident is not a type");
    return false;
  }

  return true;
}

struct p_ptr_chunk {
  u32 type_quals;
  u32 nesting;
};

struct p_decl_suf {
  bool is_array;
  u32 nesting;
  union {
    struct {
      p_ast_expr_t count_expr;
      bool has_count;
      bool is_vla;
    } array;
    struct {
      p_ast_arg_t *args;
      bool variadic;
    } func;
  } u;
};

typedef enum {
  DECL_OP_SUFFIX,
  DECL_OP_POINTER,
} p_decl_op_kind_e;

typedef struct {
  p_decl_op_kind_e kind;
  u32 nesting;
  u32 order; // stable tie-break
  union {
    struct p_decl_suf *suf;
    struct p_ptr_chunk *ptr;
  } u;
} p_decl_op_t;

static inline void p_apply_array_ty_suffix(p_ast_type_t *ty, bump_t *ast_bump,
                                           struct p_decl_suf *suf) {
  (void)suf; // if you later need qualifiers/static/vla info, use this
  p_ast_type_t out = {
      .kind = P_TY_ARRAY,
      .u.array =
          {
              .elem_ty = bump_alloc(ast_bump, sizeof(p_ast_type_t)),
              .count = bump_alloc(ast_bump, sizeof(p_ast_expr_t)),
          },
  };
  *out.u.array.elem_ty = *ty;
  *out.u.array.count = suf->u.array.count_expr;
  out.u.array.has_count = suf->u.array.has_count;
  *ty = out;
}

static inline void p_apply_func_ty_suffix(p_ast_type_t *ty, bump_t *ast_bump,
                                          struct p_decl_suf *suf) {
  p_ast_type_t out = {
      .kind = P_TY_FUNC,
      .u.func =
          {
              .ret_type = bump_alloc(ast_bump, sizeof(p_ast_type_t)),
              .args = suf->u.func.args,
          },
  };
  *out.u.func.ret_type = *ty;
  *ty = out;
}

static inline void p_apply_ty_suffix(p_ast_type_t *ty, bump_t *ast_bump,
                                     struct p_decl_suf *suf) {
  if (suf->is_array) {
    p_apply_array_ty_suffix(ty, ast_bump, suf);
  } else {
    p_apply_func_ty_suffix(ty, ast_bump, suf);
  }
}

static inline void p_apply_pointer(p_ast_type_t *ty, bump_t *ast_bump,
                                   struct p_ptr_chunk *ptr) {
  p_ast_type_t out = {
      .kind = P_TY_PTR,
      .quals = ptr->type_quals,
      .u.ptr.base_ty = bump_alloc(ast_bump, sizeof(p_ast_type_t)),
  };
  *out.u.ptr.base_ty = *ty;
  *ty = out;
}

// return >0 if a before b
static int p_cmp_decl_op(const p_decl_op_t *a, const p_decl_op_t *b) {
  if (a->nesting != b->nesting) {
    // higher nesting (inner) first
    return (a->nesting > b->nesting) ? 1 : -1;
  }

  // same nesting: suffix before pointer (C declarator precedence)
  if (a->kind != b->kind) {
    return (a->kind == DECL_OP_SUFFIX) ? 1 : -1;
  }

  // stable source order
  if (a->order < b->order)
    return 1;
  if (a->order > b->order)
    return -1;
  return 0;
}

void p_apply_decl_chunks(p_ast_type_t *ty, struct p_ptr_chunk **ptr_chunks,
                         struct p_decl_suf **suffixes, bump_t *ast_bump,
                         allocator_t *ast_allocator) {
  p_decl_op_t *ops = array_new(p_decl_op_t, ast_allocator);
  u32 ord = 0;

  for (u64 i = 0; i < array_len(*suffixes); i++) {
    p_decl_op_t op = {
        .kind = DECL_OP_SUFFIX,
        .nesting = (*suffixes)[i].nesting,
        .order = ord++,
        .u.suf = &(*suffixes)[i],
    };
    array_add(ops, op);
  }

  for (u64 i = 0; i < array_len(*ptr_chunks); i++) {
    p_decl_op_t op = {
        .kind = DECL_OP_POINTER,
        .nesting = (*ptr_chunks)[i].nesting,
        .order = ord++,
        .u.ptr = &(*ptr_chunks)[i],
    };
    array_add(ops, op);
  }

  qsort(ops, array_len(ops), sizeof(ops[0]),
        (int (*)(const void *, const void *))p_cmp_decl_op);

  for (u64 i = 0; i < array_len(ops); i++) {
    if (ops[i].kind == DECL_OP_SUFFIX) {
      printf("SUFFIX OP\n");
      p_apply_ty_suffix(ty, ast_bump, ops[i].u.suf);
    } else {
      printf("POINTER OP\n");
      p_apply_pointer(ty, ast_bump, ops[i].u.ptr);
    }
  }

  array_free(ops);
}

static bool p_resolve_declarator(p_parser_t *parser,
                                 p_ast_declarator_t *declarator,
                                 p_ast_type_t base_ty,
                                 struct p_ptr_chunk **ptr_chunks,
                                 struct p_decl_suf **suffixes) {
  declarator->type = base_ty;

  if (declarator->name == NULL) {
    if (array_len(*ptr_chunks) != 0 || array_len(*suffixes) != 0) {
      log_error("Declarator without name is not allowed to have pointer, array "
                "or function type");
      return false;
    }
    declarator->has_init = false;
    return true;
  }

  log_debug("Applying decl chunks");

  struct p_ptr_chunk *ptr_chunk;
  array_foreach(*ptr_chunks, ptr_chunk) {
    printf("POINTERCHUNK nesting: %u\n", ptr_chunk->nesting);
  }

  struct p_ptr_chunk *decl_suf;
  array_foreach(*suffixes, decl_suf) {
    printf("SUFFIXCHUNK nesting: %u\n", decl_suf->nesting);
  }

  p_apply_decl_chunks(&declarator->type, ptr_chunks, suffixes,
                      &parser->ast_bump, &parser->ast_allocator);

  array_clear(*ptr_chunks);
  array_clear(*suffixes);

  return true;
}

// TYPE PARSING
// - check for type qualifiers, storage class
// - find type (required)
// - check for pointer (tok_star), type qualifiers, storage class
//   - at this point we may encounter tok_lparen
//   - if we encounter pointer/tok_lparen stop parsing storage class
// - find name/semicolon, if semicolon is encountered, decl is over
// - find array (tok_lsquare) brackets
// - ensure all parenthesis are closed
// - check for commas
//   - if comma is found repeat from step 3, besides checking for storage class
// - find semicolon

// int *foo(int, int) ; pointer applies to return type
// int (*foo)(int, int) ; pointer applies to function
// int *foo[4] ; pointer applies to elem type
// int (*foo)[4] ; pointer applies to array

// const int *x ; const applies to int
// int *const x ; const applies to intptr
// int **const x ; const applies to inner intptr
// int *(*const x) ; const applies to inner intptr

// cur_tok is first token of decl
// cur_tok at the end is last token of the decl (semicolon/left-curly/comma
// (comma only when parsing function arguemnt))
static bool p_parse_declarators(p_parser_t *parser, p_ast_type_t *base_ty,
                                p_ast_declarator_t **declarators,
                                p_ast_arg_t *arg, p_ast_storage_class_e *sc,
                                enum p_declarator_parse_context parse_ctx) {

  struct p_ptr_chunk *ptr_chunks =
      array_new(struct p_ptr_chunk, &parser->ast_allocator);
  struct p_decl_suf *decl_suffixes =
      array_new(struct p_decl_suf, &parser->ast_allocator);

  bool sc_allowed = parse_ctx == P_TY_PARSE_CTX_DECL;
  bool type_qual_allowed = true;

  bool parsing_ptr = false;
  struct p_ptr_chunk cur_ptr_chunk = {0};

  bool parsing_declarator = false;
  p_ast_declarator_t cur_declarator = {0};

  u32 cur_nesting = 0;

  // Run infinite loop until we run into an end token
  // For regular decls the end token is semicolon/right curly
  // For function arguments the end token is comma/right parenthesis with
  // nesting 0
  for (;; p_next_token(parser)) {
    if (p_check_cur_tok(parser, P_TOKEN_SEMICOLON) ||
        p_check_cur_tok(parser, P_TOKEN_LCURLY)) {
      if (parse_ctx == P_TY_PARSE_CTX_FUNC_ARG) {
        log_error("Encountered illegal token semicolon/left curly bracket "
                  "while parsing function argument");
        return false;
      }

      if (cur_nesting != 0) {
        log_error("Encountered semicolon but expected right parenthesis while "
                  "parsing decl");
        return false;
      }

      bool resolved_decl = p_resolve_declarator(
          parser, &cur_declarator, *base_ty, &ptr_chunks, &decl_suffixes);
      if (!resolved_decl) {
        log_error("Failed to resolve declarator");
        return false;
      }
      array_add(*declarators, cur_declarator);
      break;
    } else if (parse_ctx == P_TY_PARSE_CTX_FUNC_ARG &&
               (p_check_cur_tok(parser, P_TOKEN_COMMA) ||
                (p_check_cur_tok(parser, P_TOKEN_RPAREN) &&
                 cur_nesting == 0))) {

      bool resolved_decl = p_resolve_declarator(
          parser, &cur_declarator, *base_ty, &ptr_chunks, &decl_suffixes);
      if (!resolved_decl) {
        log_error("Failed to resolve arg");
        return false;
      }

      arg->name = cur_declarator.name;
      arg->ty = cur_declarator.type;

      break;
    }

    log_debug("[PARSING-DECL] Cur tok: %s", p_cur_tok_fmt(parser));

    bool sc_parse_success = p_parse_decl_sc(*parser->cur_tok, sc);
    if (sc_parse_success) {
      if (!sc_allowed) {
        log_error(
            "Storage class specififer not allowed at this position in decl");
        return false;
      }
      continue;
    }

    bool type_qual_sucess = p_parse_type_qual(
        *parser->cur_tok,
        parsing_ptr ? &cur_ptr_chunk.type_quals : &base_ty->quals);
    if (type_qual_sucess) {
      if (!type_qual_allowed) {
        log_error("Type qualifier not allowed at this position in decl");
        return false;
      }
      continue;
    }

    if (p_check_cur_tok(parser, P_TOKEN_IDENT)) {
      if (parsing_declarator) {
        log_debug("Parsing declarator, %s", parser->cur_tok->ident_literal);
        cur_declarator.name = parser->cur_tok->ident_literal;
        sc_allowed = false;
        type_qual_allowed = false;
        if (parsing_ptr) {
          array_add(ptr_chunks, cur_ptr_chunk);
          parsing_ptr = false;
        }
      } else {
        bool base_ty_success = p_parse_base_type(parser, base_ty);
        if (!base_ty_success) {
          log_error("Failed to parse base type of decl");
          return false;
        }

        parsing_declarator = true;
      }

      continue;
    }

    // Parsing pointer and parenthesis before declarator name
    if (parsing_declarator && cur_declarator.name == NULL) {
      switch (parser->cur_tok->kind) {
      case P_TOKEN_LPAREN: {
        if (parsing_ptr) {
          array_add(ptr_chunks, cur_ptr_chunk);
        }
        parsing_ptr = false;
        sc_allowed = false;
        type_qual_allowed = false;
        cur_nesting++;
        goto end_of_decl_loop;
      } break;
      case P_TOKEN_STAR: {
        if (parsing_ptr) {
          array_add(ptr_chunks, cur_ptr_chunk);
        }
        log_debug("ENCOUNTERED STAR TOKEN");
        parsing_ptr = true;
        type_qual_allowed = true;
        sc_allowed = false;
        cur_ptr_chunk.nesting = cur_nesting;
        cur_ptr_chunk.type_quals = 0;
        goto end_of_decl_loop;
      } break;
      default: {
        log_error("Unexpected token %s while parsing decl pre-name, line: %d, "
                  "name: %s",
                  p_cur_tok_fmt(parser), parser->cur_tok->line,
                  cur_declarator.name);
        return false;
      } break;
      }
    }
    // Parsing array, functions, closing parenthesis, comma after declarator
    // name
    else if (parsing_declarator && cur_declarator.name != NULL) {
      switch (parser->cur_tok->kind) {
      case P_TOKEN_RPAREN: {
        cur_nesting--;
        goto end_of_decl_loop;
      } break;
      case P_TOKEN_LSQUARE: {
        log_debug("Found array");
        struct p_decl_suf decl_suf = {.is_array = true, .nesting = cur_nesting};
        if (p_expect_peek_tok(parser, P_TOKEN_RSQUARE)) {
          decl_suf.u.array.has_count = false;
        } else {
          p_next_token(parser);

          bool count_success =
              p_parse_expr(parser, &decl_suf.u.array.count_expr);
          if (!count_success) {
            log_error("Failed to parse count of array");
            return false;
          }

          decl_suf.u.array.has_count = true;

          if (!p_expect_peek_tok(parser, P_TOKEN_RSQUARE)) {
            log_error("Expected right square token after array count expr");
            return false;
          }
        }
        array_add(decl_suffixes, decl_suf);
        goto end_of_decl_loop;
      } break;
      case P_TOKEN_LPAREN: {
        struct p_decl_suf decl_suf = {
            .is_array = false,
            .nesting = cur_nesting,
            .u.func.args = array_new(p_ast_arg_t, &parser->ast_allocator),
        };

        bool args_success = p_parse_args_list(parser, &decl_suf.u.func.args);
        if (!args_success) {
          log_error("Failed to parse args of function type");
          return false;
        }

        array_add(decl_suffixes, decl_suf);
        goto end_of_decl_loop;
      } break;
      case P_TOKEN_COMMA: {
        log_debug("Found comma");
        ASSERT(parse_ctx != P_TY_PARSE_CTX_FUNC_ARG,
               "Handled comma in func arg the wrong way");

        if (cur_nesting != 0) {
          log_error("Encountered comma but expected right parenthesis while "
                    "parsing decl");
          return false;
        }

        bool resolved_decl = p_resolve_declarator(
            parser, &cur_declarator, *base_ty, &ptr_chunks, &decl_suffixes);
        if (!resolved_decl) {
          log_error("Failed to resolve declarator");
          return false;
        }
        array_add(*declarators, cur_declarator);

        memset(&cur_declarator, 0, sizeof(p_ast_declarator_t));

        goto end_of_decl_loop;

        // TODO: Handle ptr chunks and decl suffixes

      } break;
      case P_TOKEN_ASSIGN: {
        if (parse_ctx != P_TY_PARSE_CTX_DECL) {
          log_error("Unexpected token assign while parsing %s",
                    parse_ctx == P_TY_PARSE_CTX_REC_FIELD ? "field"
                                                          : "function arg");
          return false;
        }

        if (cur_nesting != 0) {
          log_error("Encountered assign but expected right parenthesis while "
                    "parsing decl");
          return false;
        }

        p_next_token(parser);

        bool init_success = p_parse_init(parser, &cur_declarator.init);
        if (!init_success) {
          log_error("Failed to parse init for decl");
          return false;
        }

        cur_declarator.has_init = true;

        if (!p_check_peek_tok(parser, P_TOKEN_COMMA) &&
            !p_check_peek_tok(parser, P_TOKEN_SEMICOLON)) {
          log_error("Expected comma or semicolon after assign");
          return false;
        }

        goto end_of_decl_loop;
      }
      default: {
        log_error("Unexpected token %s while parsing decl post-name",
                  p_cur_tok_fmt(parser));
        return false;
      } break;
      }
    }

  end_of_decl_loop:
    continue;
  }

  if (array_len(decl_suffixes) == 0) {
    struct p_ptr_chunk *chunk;
    array_foreach(ptr_chunks, chunk) {}
  }

  return true;
}

static bool p_parse_stmt(p_parser_t *parser, p_ast_stmt_t *stmt);

static bool p_parse_base_expr(p_parser_t *parser, p_ast_expr_t *expr) {
  switch (parser->cur_tok->kind) {
  case P_TOKEN_IDENT: {
    expr->kind = P_EX_IDENT;
    expr->u.ident = parser->cur_tok->ident_literal;
  } break;
  case P_TOKEN_INTEGER: {
    expr->kind = P_EX_INT;
    expr->u.integer = parser->cur_tok->int_literal;
  } break;
  case P_TOKEN_FLOAT: {
    expr->kind = P_EX_FLOAT;
    expr->u.fp = parser->cur_tok->float_literal;
  } break;
  case P_TOKEN_STRING: {
    expr->kind = P_EX_STR;
    expr->u.str = parser->cur_tok->string_literal;
  } break;
  case P_TOKEN_CHAR: {
    expr->kind = P_EX_CHAR;
    expr->u.ch = parser->cur_tok->char_literal;
  } break;
  case P_TOKEN_LPAREN: {

    if (p_check_peek_tok(parser, P_TOKEN_IDENT)) {
    }
  } break;
  case P_TOKEN_AMP:
  case P_TOKEN_BANG:
  case P_TOKEN_PLUS:
  case P_TOKEN_MINUS:
  case P_TOKEN_TILDE:
  case P_TOKEN_STAR: {
    p_tok_t cur_tok = *parser->cur_tok;
    expr->kind = P_EX_UNARY;
    expr->u.unary_op.ex = bump_alloc(&parser->ast_bump, sizeof(p_ast_expr_t));
    switch (parser->cur_tok->kind) {
    case P_TOKEN_AMP: {
      expr->u.unary_op.op = P_UOP_ADDR;
    } break;
    case P_TOKEN_BANG: {
      expr->u.unary_op.op = P_UOP_LNOT;
    } break;
    case P_TOKEN_PLUS: {
      expr->u.unary_op.op = P_UOP_POS;
    } break;
    case P_TOKEN_MINUS: {
      expr->u.unary_op.op = P_UOP_NEG;
    } break;
    case P_TOKEN_TILDE: {
      expr->u.unary_op.op = P_UOP_BNOT;
    } break;
    case P_TOKEN_STAR: {
      expr->u.unary_op.op = P_UOP_DEREF;
    } break;
    default: {
      log_error("Invalid token %s at beginning of unary operator",
                p_cur_tok_fmt(parser));
      return false;
    } break;
    }

    p_next_token(parser);
    bool expr_success = p_parse_expr(parser, expr->u.unary_op.ex);
    if (!expr_success) {
      log_error("Failed to parse expr after token %s",
                p_tok_fmt(&cur_tok, &parser->ast_allocator).string);
      return false;
    }
  } break;
  case P_TOKEN_MINUS_MINUS:
  case P_TOKEN_PLUS_PLUS: {
    expr->kind = p_check_cur_tok(parser, P_TOKEN_PLUS_PLUS) ? P_EX_PRE_INC
                                                            : P_EX_POST_INC;
    expr->u.pre_post_dec_inc =
        bump_alloc(&parser->ast_bump, sizeof(p_ast_expr_t));
    bool expr_success = p_parse_expr(parser, expr->u.pre_post_dec_inc);
    if (!expr_success) {
      log_error("Failed to past expr after prefix %s",
                expr->kind == P_EX_PRE_INC ? "increment" : "decrement");
      return false;
    }
  } break;
  case P_TOKEN_INVALID:
  case P_TOKEN_EOF:
  case P_TOKEN_COMMA:
  case P_TOKEN_SEMICOLON:
  case P_TOKEN_COLON:
  case P_TOKEN_QUESTION:
  case P_TOKEN_DOT:
  case P_TOKEN_DOT_DOT_DOT:
  case P_TOKEN_HASH:
  case P_TOKEN_HASH_HASH:
  case P_TOKEN_RPAREN:
  case P_TOKEN_LSQUARE:
  case P_TOKEN_RSQUARE:
  case P_TOKEN_LCURLY:
  case P_TOKEN_RCURLY:
  case P_TOKEN_SLASH:
  case P_TOKEN_PERCENT:
  case P_TOKEN_PIPE:
  case P_TOKEN_CARET:
  case P_TOKEN_ASSIGN:
  case P_TOKEN_LT:
  case P_TOKEN_GT:
  case P_TOKEN_ARROW:
  case P_TOKEN_PLUS_ASSIGN:
  case P_TOKEN_MINUS_ASSIGN:
  case P_TOKEN_STAR_ASSIGN:
  case P_TOKEN_SLASH_ASSIGN:
  case P_TOKEN_PERCENT_ASSIGN:
  case P_TOKEN_AMP_ASSIGN:
  case P_TOKEN_PIPE_ASSIGN:
  case P_TOKEN_CARET_ASSIGN:
  case P_TOKEN_LT_LT:
  case P_TOKEN_GT_GT:
  case P_TOKEN_LT_LT_ASSIGN:
  case P_TOKEN_GT_GT_ASSIGN:
  case P_TOKEN_EQ_EQ:
  case P_TOKEN_BANG_EQ:
  case P_TOKEN_LT_EQ:
  case P_TOKEN_GT_EQ:
  case P_TOKEN_AMP_AMP:
  case P_TOKEN_PIPE_PIPE: {
    log_error("Failed to parse expr: %s", p_cur_tok_fmt(parser));
    return false;
  } break;
  }
  return true;
}

static bool p_parse_rec_member(p_parser_t *parser,
                               struct p_ast_expr_member *mem_expr) {}

// clang-format off
static bool p_tok_to_bin_op(p_parser_t *parser, const p_tok_t *tok, p_binary_op_e *bin_op) {
  switch (tok->kind) {
  // Infix-Logic
  case P_TOKEN_EQ_EQ:     *bin_op = P_BOP_EQ;    break;
  case P_TOKEN_BANG_EQ:   *bin_op = P_BOP_NEQ;   break;
  case P_TOKEN_LT:        *bin_op = P_BOP_LT;    break;
  case P_TOKEN_GT:        *bin_op = P_BOP_GT;    break;
  case P_TOKEN_LT_EQ:     *bin_op = P_BOP_LT_EQ; break;
  case P_TOKEN_GT_EQ:     *bin_op = P_BOP_GT_EQ; break;
  case P_TOKEN_AMP_AMP:   *bin_op = P_BOP_LAND;  break;
  case P_TOKEN_PIPE_PIPE: *bin_op = P_BOP_LOR;   break;
  // Infix-Arithmentic
  case P_TOKEN_PLUS:      *bin_op = P_BOP_ADD;   break;
  case P_TOKEN_MINUS:     *bin_op = P_BOP_SUB;   break;
  case P_TOKEN_STAR:      *bin_op = P_BOP_MUL;   break;
  case P_TOKEN_SLASH:     *bin_op = P_BOP_DIV;   break;
  case P_TOKEN_PERCENT:   *bin_op = P_BOP_MOD;   break;
  // Infix-Bitwise
  case P_TOKEN_LT_LT:     *bin_op = P_BOP_SHL;   break;
  case P_TOKEN_GT_GT:     *bin_op = P_BOP_SHR;   break;
  case P_TOKEN_CARET:     *bin_op = P_BOP_BXOR;  break;
  case P_TOKEN_AMP:       *bin_op = P_BOP_BAND;  break;
  case P_TOKEN_PIPE:      *bin_op = P_BOP_BOR;   break;
  default: {
    log_error("Cannot convert token to binary operator");
    return false;
  }
  }

  return true;
}
// clang-format on

static bool p_parse_expr(p_parser_t *parser, p_ast_expr_t *expr) {
  p_ast_expr_t left_expr = {0};
  bool left_success = p_parse_base_expr(parser, &left_expr);
  if (!left_success) {
    log_error("Failed to parse base expr");
    return false;
  }

  switch (parser->cur_tok->kind) {
    // Special
  case P_TOKEN_DOT:
  case P_TOKEN_ARROW:
    // Postfix
  case P_TOKEN_LPAREN:
  case P_TOKEN_LSQUARE:
  case P_TOKEN_PLUS_PLUS:
  case P_TOKEN_MINUS_MINUS:
  // Infix
  case P_TOKEN_ASSIGN:
  // Infix-Arithmetic-Assign
  case P_TOKEN_PLUS_ASSIGN:
  case P_TOKEN_MINUS_ASSIGN:
  case P_TOKEN_STAR_ASSIGN:
  case P_TOKEN_SLASH_ASSIGN:
  case P_TOKEN_PERCENT_ASSIGN:
  // Infix-Bitwise-Assign
  case P_TOKEN_LT_LT_ASSIGN:
  case P_TOKEN_GT_GT_ASSIGN:
  case P_TOKEN_CARET_ASSIGN:
  case P_TOKEN_AMP_ASSIGN:
  case P_TOKEN_PIPE_ASSIGN: {
    
  } break;
  // Infix-Logic
  case P_TOKEN_EQ_EQ:
  case P_TOKEN_BANG_EQ:
  case P_TOKEN_LT:
  case P_TOKEN_GT:
  case P_TOKEN_LT_EQ:
  case P_TOKEN_GT_EQ:
  case P_TOKEN_AMP_AMP:
  case P_TOKEN_PIPE_PIPE:
  // Infix-Arithmentic
  case P_TOKEN_PLUS:
  case P_TOKEN_MINUS:
  case P_TOKEN_STAR:
  case P_TOKEN_SLASH:
  case P_TOKEN_PERCENT:
  // Infix-Bitwise
  case P_TOKEN_LT_LT:
  case P_TOKEN_GT_GT:
  case P_TOKEN_CARET:
  case P_TOKEN_AMP:
  case P_TOKEN_PIPE: {
    
  } break;
  }
}

static bool p_parse_init(p_parser_t *parser, p_ast_init_t *init) {
  if (p_expect_cur_tok(parser, P_TOKEN_LCURLY)) {
    init->kind = P_INIT_LIST;
    init->u.list = array_new(p_ast_init_item_t, &parser->ast_allocator);

    while (!p_check_cur_tok(parser, P_TOKEN_RCURLY)) {
      p_ast_init_item_t item = {0};
      item.desigs = array_new(p_ast_init_desig_t, &parser->ast_allocator);

      bool found_desig = false;
      enum p_ast_init_desig_kind desig_kind = P_INIT_DESIG_FIELD;
      union p_ast_init_desig_var desig_var = {0};

      if (p_check_cur_tok(parser, P_TOKEN_DOT)) {
        found_desig = true;
        desig_kind = P_INIT_DESIG_FIELD;
      } else if (p_check_cur_tok(parser, P_TOKEN_LSQUARE)) {
        found_desig = true;
        desig_kind = P_INIT_DESIG_INDEX;
      }

      if (found_desig) {
        do {
          p_next_token(parser);

          switch (desig_kind) {
          case P_INIT_DESIG_FIELD: {
            const char *ident = parser->cur_tok->ident_literal;
            if (!p_expect_cur_tok(parser, P_TOKEN_IDENT)) {
              log_error("Expected field name after dot in initializer");
              return false;
            }

            desig_var.field = ident;
          } break;
          case P_INIT_DESIG_INDEX: {
            desig_var.index =
                bump_alloc(&parser->ast_bump, sizeof(p_ast_expr_t));
            bool idx_success = p_parse_expr(parser, desig_var.index);
            if (!idx_success) {
              log_error("Failed to parse expr for index designator");
              return false;
            }

            log_debug("Cur tok: %s", p_cur_tok_fmt(parser));
            log_debug("Peek tok: %s", p_peek_tok_fmt(parser));

            p_next_token(parser);

            if (!p_expect_cur_tok(parser, P_TOKEN_RSQUARE)) {
              log_error("Expected right square bracket after index expression, "
                        "received token: %s",
                        p_peek_tok_fmt(parser));
            }
          } break;
          }

          log_debug("Cur tok post desig: %s", p_cur_tok_fmt(parser));

          if (p_check_cur_tok(parser, P_TOKEN_DOT)) {
            found_desig = true;
            desig_kind = P_INIT_DESIG_FIELD;
          } else if (p_check_cur_tok(parser, P_TOKEN_LSQUARE)) {
            found_desig = true;
            desig_kind = P_INIT_DESIG_INDEX;
          } else {
            found_desig = false;
          }

          array_add(item.desigs,
                    (p_ast_init_desig_t){.kind = desig_kind, .u = desig_var});
        } while (found_desig);

        if (!p_expect_cur_tok(parser, P_TOKEN_ASSIGN)) {
          log_error("Expected assign after field designators in initializer");
          return false;
        }
      }

      item.value = bump_alloc(&parser->ast_bump, sizeof(p_ast_init_t));
      bool item_init_value_success = p_parse_init(parser, item.value);
      if (!item_init_value_success) {
        log_error("Failed to parse init item for designator in initializer");
        return false;
      }

      if (p_expect_peek_tok(parser, P_TOKEN_COMMA)) {
        p_next_token(parser);
      } else if (!p_expect_peek_tok(parser, P_TOKEN_RCURLY)) {
        log_error("Expected comma or rcurly at end of initializer");
      }

      array_add(init->u.list, item);
    }

    return true;
  }

  init->kind = P_INIT_EXPR;
  init->u.expr = bump_alloc(&parser->ast_bump, sizeof(p_ast_expr_t));
  bool expr_success = p_parse_expr(parser, init->u.expr);
  if (!expr_success) {
    log_error("Failed to parse init expr");
    return false;
  }

  return true;
}

// cur tok is lcurly
static bool p_parse_compound_stmt(p_parser_t *parser,
                                  p_ast_block_item_t **block_items) {
  p_next_token(parser);

  while (parser->cur_tok->kind != P_TOKEN_RCURLY) {
    p_ast_block_item_t block_item = {0};

    p_ast_stmt_t block_stmt = {0};
    bool stmt_success = p_parse_stmt(parser, &block_stmt);
    if (!stmt_success) {
      log_error("Failed to parse block item");
      return false;
    }

    if (block_stmt.kind == P_ST_DECL) {
      block_item.is_decl = true;
      block_item.u.decl = block_stmt.u.decl_stmt.decl;
    } else {
      block_item.is_decl = false;
      block_item.u.stmt = bump_alloc(&parser->ast_bump, sizeof(p_ast_stmt_t));
      *block_item.u.stmt = block_stmt;
    }

    array_add(*block_items, block_item);

    p_next_token(parser);
  }
  return true;
}

static bool p_parse_if_stmt(p_parser_t *parser, struct p_ast_stmt_if *if_stmt) {
  p_next_token(parser);

  if (!p_expect_cur_tok(parser, P_TOKEN_LPAREN)) {
    log_error("Expected token left parenthesis after if, received token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  bool cond_success = p_parse_expr(parser, &if_stmt->cond);
  if (!cond_success) {
    log_error("Failed to parse condition");
    return false;
  }

  if (!p_expect_cur_tok(parser, P_TOKEN_RPAREN)) {
    log_error("Expected token right parenthesis after if condition, received "
              "token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  if_stmt->then_branch = bump_alloc(&parser->ast_bump, sizeof(p_ast_stmt_t));
  bool then_stmt_success = p_parse_stmt(parser, if_stmt->then_branch);
  if (!then_stmt_success) {
    log_error("Failed to parse then branch");
    return false;
  }

  if (p_check_peek_tok(parser, P_TOKEN_IDENT) &&
      str_eq(parser->peek_tok->ident_literal, "else")) {
    // cur tok is else
    p_next_token(parser);

    // cur tok is stmt after else
    p_next_token(parser);

    if_stmt->else_branch = bump_alloc(&parser->ast_bump, sizeof(p_ast_stmt_t));
    bool else_branch_success = p_parse_stmt(parser, if_stmt->else_branch);
    if (!else_branch_success) {
      log_error("Failed to parse else branch");
      return false;
    }
  }

  return false;
}

static bool p_parse_for_stmt(p_parser_t *parser,
                             struct p_ast_stmt_for *for_stmt) {
  p_next_token(parser);

  if (!p_expect_cur_tok(parser, P_TOKEN_LPAREN)) {
    log_error("Expected token left parenthesis after for, received token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  p_ast_stmt_t init_stmt = {0};

  bool init_success = p_parse_stmt(parser, &init_stmt);
  if (!init_success) {
    log_error("Failed to parse for-init");
    return false;
  }

  if (init_stmt.kind == P_ST_EXPR) {
    for_stmt->has_init_expr = true;
    for_stmt->init_expr = init_stmt.u.expr;
  } else if (init_stmt.kind == P_ST_DECL) {
    for_stmt->has_init_expr = false;
    for_stmt->init_decl = init_stmt.u.decl_stmt.decl;
  } else {
    log_error("Invalid statement for init expression of for loop");
    return false;
  }

  if (for_stmt->has_init_expr &&
      !p_expect_peek_tok(parser, P_TOKEN_SEMICOLON)) {
    log_error("Expected semicolon after for loop init expr");
    return false;
  }

  p_next_token(parser);

  p_ast_expr_t for_cond = {0};
  bool cond_success = p_parse_expr(parser, &for_cond);
  if (!cond_success) {
    log_error("Failed to parse for-cond");
    return false;
  }

  for_stmt->cond = for_cond;

  if (!p_expect_peek_tok(parser, P_TOKEN_SEMICOLON)) {
    log_error("Expected semicolon after for loop condition expr");
    return false;
  }

  p_next_token(parser);

  p_ast_expr_t for_post = {0};
  bool post_success = p_parse_expr(parser, &for_post);
  if (!post_success) {
    log_error("Failed to parse for-post");
    return false;
  }

  for_stmt->post = for_post;

  p_next_token(parser);

  if (!p_expect_cur_tok(parser, P_TOKEN_RPAREN)) {
    log_error("Expected token right parenthesis after for loop post "
              "expression, received "
              "token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  p_next_token(parser);

  p_ast_stmt_t body_stmt = {0};

  bool body_success = p_parse_stmt(parser, &body_stmt);
  if (!body_success) {
    log_error("Expected body stmt after for loop");
    return false;
  }

  for_stmt->body = bump_alloc(&parser->ast_bump, sizeof(p_ast_stmt_t));
  *for_stmt->body = body_stmt;

  return false;
}

static bool p_parse_while_stmt(p_parser_t *parser,
                               struct p_ast_stmt_while *while_stmt) {
  p_next_token(parser);

  if (!p_expect_cur_tok(parser, P_TOKEN_LPAREN)) {
    log_error("Expected token left parenthesis after while, received token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  p_ast_expr_t cond_expr = {0};

  bool cond_success = p_parse_expr(parser, &cond_expr);
  if (!cond_success) {
    log_error("Failed to parse while-cond");
    return false;
  }

  while_stmt->cond = cond_expr;

  p_next_token(parser);

  if (!p_expect_cur_tok(parser, P_TOKEN_RPAREN)) {
    log_error(
        "Expected token right parenthesis after while condition, received "
        "token: %s",
        p_cur_tok_fmt(parser));
    return false;
  }

  p_next_token(parser);

  p_ast_stmt_t body_stmt = {0};

  bool body_success = p_parse_stmt(parser, &body_stmt);
  if (!body_success) {
    log_error("Expected body stmt after while loop");
    return false;
  }

  while_stmt->body = bump_alloc(&parser->ast_bump, sizeof(p_ast_stmt_t));
  *while_stmt->body = body_stmt;

  return false;
}

static bool p_parse_switch_stmt(p_parser_t *parser,
                                struct p_ast_stmt_switch *switch_stmt) {
  return false;
}

static bool p_parse_do_while_stmt(p_parser_t *parser,
                                  struct p_ast_stmt_do_while *do_while_stmt) {
  return false;
}

static bool p_parse_return_stmt(p_parser_t *parser,
                                struct p_ast_stmt_return *return_stmt) {
  if (p_expect_peek_tok(parser, P_TOKEN_SEMICOLON)) {
    return_stmt->has_return_expr = false;
    return true;
  }

  p_next_token(parser);

  bool return_expr_success = p_parse_expr(parser, &return_stmt->expr);
  if (!return_expr_success) {
    log_error("Failed to parse return expr");
    return false;
  }

  if (!p_expect_peek_tok(parser, P_TOKEN_SEMICOLON)) {
    log_error("Expected semicolon after return, received token: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  return_stmt->has_return_expr = true;
  return true;
}

static bool p_parse_decl(p_parser_t *parser, p_ast_decl_t *decl);

static bool p_parse_stmt(p_parser_t *parser, p_ast_stmt_t *stmt) {
  switch (parser->cur_tok->kind) {
  case P_TOKEN_IDENT: {
    const char *ident = parser->cur_tok->ident_literal;

    bool success = false;

    if (str_eq(ident, "if")) {
      stmt->kind = P_ST_IF;
      success = p_parse_if_stmt(parser, &stmt->u.if_stmt);
    } else if (str_eq(ident, "for")) {
      stmt->kind = P_ST_FOR;
      success = p_parse_for_stmt(parser, &stmt->u.for_stmt);
    } else if (str_eq(ident, "while")) {
      stmt->kind = P_ST_WHILE;
      success = p_parse_while_stmt(parser, &stmt->u.while_stmt);
    } else if (str_eq(ident, "switch")) {
      stmt->kind = P_ST_SWITCH;
      success = p_parse_switch_stmt(parser, &stmt->u.switch_stmt);
    } else if (str_eq(ident, "do")) {
      stmt->kind = P_ST_DO_WHILE;
      success = p_parse_do_while_stmt(parser, &stmt->u.do_while);
    } else if (str_eq(ident, "return")) {
      stmt->kind = P_ST_RETURN;
      success = p_parse_return_stmt(parser, &stmt->u.return_stmt);
    } else if (str_eq(ident, "break")) {
      stmt->kind = P_ST_BREAK;
    } else if (str_eq(ident, "continue")) {
      stmt->kind = P_ST_CONTINUE;
    } else if (str_eq(ident, "goto")) {
      TODO();
    } else if (p_is_type_ident(parser, ident, NULL) ||
               p_parse_decl_sc(*parser->cur_tok, NULL) ||
               p_parse_type_qual(*parser->cur_tok, NULL)) {
      stmt->kind = P_ST_DECL;
      success = p_parse_decl(parser, &stmt->u.decl_stmt.decl);
    }

    // TODO: Parse decl

    if (!success) {
      stmt->kind = P_ST_EXPR;
      success = p_parse_expr(parser, &stmt->u.expr);
    }

    return success;
  } break;
  case P_TOKEN_LCURLY: {
    stmt->kind = P_ST_COMPOUND;
    stmt->u.compound_items =
        array_new(p_ast_block_item_t, &parser->ast_allocator);
    bool success = p_parse_compound_stmt(parser, &stmt->u.compound_items);
    if (!success) {
      log_error("Failed to parse compound stmt");
      return false;
    }
  } break;
  case P_TOKEN_SEMICOLON: {
    stmt->kind = P_ST_EMPTY;
    return true;
  } break;
  case P_TOKEN_LPAREN:
  case P_TOKEN_PLUS:
  case P_TOKEN_MINUS:
  case P_TOKEN_STAR:
  case P_TOKEN_BANG:
  case P_TOKEN_TILDE:
  case P_TOKEN_AMP:
  case P_TOKEN_PLUS_PLUS:
  case P_TOKEN_MINUS_MINUS:
  case P_TOKEN_INTEGER:
  case P_TOKEN_FLOAT:
  case P_TOKEN_STRING:
  case P_TOKEN_CHAR: {
    stmt->kind = P_ST_EXPR;
    bool success = p_parse_expr(parser, &stmt->u.expr);
    if (!success) {
      log_error("failed to parse expr");
      return success;
    }
    log_debug("parsing expr");
  } break;
  case P_TOKEN_COMMA:
  case P_TOKEN_COLON:
  case P_TOKEN_QUESTION:
  case P_TOKEN_DOT:
  case P_TOKEN_DOT_DOT_DOT:
  case P_TOKEN_HASH:
  case P_TOKEN_HASH_HASH:
  case P_TOKEN_LSQUARE:
  case P_TOKEN_ASSIGN:
  case P_TOKEN_SLASH:
  case P_TOKEN_PERCENT:
  case P_TOKEN_PIPE:
  case P_TOKEN_CARET:
  case P_TOKEN_LT:
  case P_TOKEN_GT:
  case P_TOKEN_RPAREN:
  case P_TOKEN_RSQUARE:
  case P_TOKEN_RCURLY:
  case P_TOKEN_ARROW:
  case P_TOKEN_PLUS_ASSIGN:
  case P_TOKEN_MINUS_ASSIGN:
  case P_TOKEN_STAR_ASSIGN:
  case P_TOKEN_SLASH_ASSIGN:
  case P_TOKEN_PERCENT_ASSIGN:
  case P_TOKEN_AMP_ASSIGN:
  case P_TOKEN_PIPE_ASSIGN:
  case P_TOKEN_CARET_ASSIGN:
  case P_TOKEN_LT_LT:
  case P_TOKEN_GT_GT:
  case P_TOKEN_LT_LT_ASSIGN:
  case P_TOKEN_GT_GT_ASSIGN:
  case P_TOKEN_EQ_EQ:
  case P_TOKEN_BANG_EQ:
  case P_TOKEN_LT_EQ:
  case P_TOKEN_GT_EQ:
  case P_TOKEN_AMP_AMP:
  case P_TOKEN_PIPE_PIPE:
  case P_TOKEN_INVALID: {
    log_error("Invalid token %s at beginning of stmt", p_cur_tok_fmt(parser));
    return false;
  } break;
  case P_TOKEN_EOF: {
    panic("UNREACHABLE EOF");
    return false;
  } break;
  }

  return true;
}

static bool p_parse_decl(p_parser_t *parser, p_ast_decl_t *decl) {
  decl->decls = array_new(p_ast_declarator_t, &parser->ast_allocator);
  bool args_success =
      p_parse_declarators(parser, &decl->base_type, &decl->decls, NULL,
                          &decl->sc, P_TY_PARSE_CTX_DECL);
  if (!args_success) {
    log_error("Failed to parse type for typedef");
    return false;
  }

  return true;
}

static bool p_parse_extern(p_parser_t *parser, p_ast_extern_t *external) {
  p_ast_decl_t decl = {0};
  bool success = p_parse_decl(parser, &decl);
  if (!success) {
    log_error("Failed to parse decl for extern");
    return false;
  }

  external->kind = P_EXT_DECL;
  external->u.decl = decl;

  if (p_check_cur_tok(parser, P_TOKEN_LCURLY)) {
    external->kind = P_EXT_FUNC_DEF;
    external->u.func.decl = decl;
    p_ast_block_item_t *block_items =
        array_new(p_ast_block_item_t, &parser->ast_allocator);
    bool success = p_parse_compound_stmt(parser, &block_items);
    if (!success) {
      log_error("Failed to parse function block");
      return false;
    }
    p_ast_stmt_t body_stmt = {.kind = P_ST_COMPOUND,
                              .u.compound_items = block_items};
    external->u.func.body = body_stmt;
  }

  return true;
}

void p_parser_parse(p_parser_t *parser, const p_tok_t *tokens,
                    const char *filename, const char *source) {
  parser->filename = filename;
  parser->source = source;
  parser->tokens = tokens;

  bump_reset(&parser->ast_bump);
  parser->ast.externs = array_new(p_ast_extern_t, &parser->ast_allocator);

  parser->peek_tok = parser->tokens;
  if (parser->peek_tok->kind == P_TOKEN_EOF) {
    return;
  }

  p_next_token(parser);

  while (parser->cur_tok->kind != P_TOKEN_EOF) {
    p_ast_extern_t external = {0};
    p_parse_extern(parser, &external);
    array_add(parser->ast.externs, external);

    p_next_token(parser);
  }
}
