#include "../include/parser.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilc/panic.h"
#include "lilc/str.h"
#include "lilc/todo.h"

void p_parser_init(p_parser_t *parser) {
  bump_init(&parser->ast_bump, 16000);
  bump_allocator_init(&parser->ast_allocator, &parser->ast_bump);
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

static inline bool p_check_cur_tok(p_parser_t *parser, p_tok_kind_e tok_kind) {
  return parser->cur_tok->kind == tok_kind;
}

static inline bool p_check_peek_tok(p_parser_t *parser, p_tok_kind_e tok_kind) {
  return parser->peek_tok->kind == tok_kind;
}

static i32 p_builtin_type_idx(const char *ident);

static bool p_parse_stmt(p_parser_t *parser, p_ast_stmt_t *stmt);

static bool p_parse_expr(p_parser_t *parser, p_ast_expr_t *expr) {
  switch (parser->cur_tok->kind) {
  case P_TOKEN_IDENT: {

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
  case P_TOKEN_PLUS: {

  } break;
  case P_TOKEN_MINUS: {

  } break;
  case P_TOKEN_STAR: {

  } break;
  case P_TOKEN_AMP: {

  } break;
  case P_TOKEN_BANG: {

  } break;
  case P_TOKEN_TILDE: {

  } break;
  case P_TOKEN_PLUS_PLUS: {

  } break;
  case P_TOKEN_MINUS_MINUS: {

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

static bool p_parse_init(p_parser_t *parser, p_ast_init_t *init) {
  if (p_check_cur_tok(parser, P_TOKEN_LCURLY)) {
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

      p_next_token(parser);

      if (found_desig) {
        do {
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
            bool idx_success = p_parse_expr(parser, desig_var.index);
            if (!idx_success) {
              log_error("Failed to parse expr for index designator");
              return false;
            }

            p_next_token(parser);

            if (!p_expect_peek_tok(parser, P_TOKEN_RSQUARE)) {
              log_error("Expected right square bracket after index expression, "
                        "received token: %s",
                        p_peek_tok_fmt(parser));
            }

          } break;
          }

          p_next_token(parser);

          if (p_check_cur_tok(parser, P_TOKEN_DOT)) {
            found_desig = true;
            desig_kind = P_INIT_DESIG_FIELD;
          } else if (p_check_cur_tok(parser, P_TOKEN_LSQUARE)) {
            found_desig = true;
            desig_kind = P_INIT_DESIG_INDEX;
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
    } else if (p_builtin_type_idx(ident) != -1) {
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

static bool p_parse_type(p_parser_t *parser, p_ast_type_t *type) {
  p_tok_t first_tok = *parser->cur_tok;
  if (!p_expect_cur_tok(parser, P_TOKEN_IDENT)) {
    log_error("Expected token ident at beginning of type, received: %s",
              p_cur_tok_fmt(parser));
    return false;
  }

  const char *ty_ident = first_tok.ident_literal;
  // TODO: track declared types and allow using them
  i32 builtin_ty_idx = p_builtin_type_idx(ty_ident);
  if (builtin_ty_idx == -1) {
    log_error("Unknown type %s", ty_ident);
    return false;
  }

  *type = p_ident_to_type(ty_ident, builtin_ty_idx);

  return true;
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

static bool p_parse_decl_sc(p_parser_t *parser, p_ast_storage_class_e *sc) {
  if (!p_check_cur_tok(parser, P_TOKEN_IDENT)) {
    *sc = P_SC_NONE;
    return false;
  }

  for (u64 i = 0;
       i < sizeof(glob_storage_classes) / sizeof(struct p_storage_class); i++) {
    if (str_eq(parser->cur_tok->ident_literal, glob_storage_classes[i].name)) {
      *sc = glob_storage_classes[i].sc;
      return true;
    }
  }

  *sc = P_SC_NONE;

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

static bool p_parse_decl_qual(p_parser_t *parser, u32 *quals) {
  if (!p_check_cur_tok(parser, P_TOKEN_IDENT)) {
    return false;
  }

  for (u64 i = 0; i < sizeof(glob_type_quals) / sizeof(struct p_type_qual);
       i++) {
    if (str_eq(parser->cur_tok->ident_literal, glob_type_quals[i].name)) {
      *quals |= glob_type_quals[i].qual;
      return true;
    }
  }

  return false;
}

static bool p_parse_decl(p_parser_t *parser, p_ast_decl_t *decl) {
  if (p_check_cur_tok(parser, P_TOKEN_IDENT) &&
      str_eq(parser->cur_tok->ident_literal, "typedef")) {
    decl->sc = P_SC_TYPEDEF;

    p_next_token(parser);

    bool found_ty = false;
    i32 ty_idx = 0;
    const char *ty_name_ident = NULL;

    for (;;) {
      bool qual_success = p_parse_decl_qual(parser, &decl->quals);
      const char *ident = parser->cur_tok->ident_literal;
      if (!qual_success) {
        if (p_check_cur_tok(parser, P_TOKEN_IDENT)) {
          // TODO: Check for builtin and typedef names
          // TODO: Advanced types like pointers, arrays...
          i32 builtin_type_index = p_builtin_type_idx(ident);
          if (builtin_type_index != -1) {
            if (found_ty) {
              log_error("Encountered second type in typedef");
              return false;
            }
            found_ty = true;
            ty_idx = builtin_type_index;
          } else {
            ty_name_ident = ident;
            break;
          }
        } else {
          log_error("Expected type qualifier, type of typedef name, received token: %s", p_cur_tok_fmt(parser));
        }
      }

      p_next_token(parser);
    }

    decl->base_type = p_ident_to_type(NULL, ty_idx);
    decl->decls = array_new_capacity(p_ast_declarator_t, 2, &parser->ast_allocator);
    decl->decls[0].type = decl->base_type;
    decl->decls[0].has_init = false;
    decl->decls[0].name = ty_name_ident;

    if (!p_expect_peek_tok(parser, P_TOKEN_SEMICOLON)) {
      log_error("Expected semicolon after typedef");
      return false;
    }

    return true;
  }

  p_ast_type_t ty = {0};
  bool found_ty = false;

  for (;;) {
    bool sc_success = p_parse_decl_sc(parser, &decl->sc);
    if (sc_success) {
      p_next_token(parser);
    }
    bool qual_success = p_parse_decl_qual(parser, &decl->quals);
    if (qual_success) {
      p_next_token(parser);
    }

    if (!sc_success && !qual_success) {
      if (found_ty) {
        break;
      }

      // TODO: pass sc and qual to function and make it also parse the stuff
      bool ty_success = p_parse_type(parser, &ty);
      if (!ty_success) {
        log_error("Failed to pass decl, expected storage class, type qualifier "
                  "or type, received token: %s",
                  p_cur_tok_fmt(parser));
        return false;
      }
      found_ty = true;
    }
  }

  if (p_check_cur_tok(parser, P_TOKEN_SEMICOLON)) {
    log_debug("decl without name");
    return true;
  }

  p_ast_declarator_t *declarators =
      array_new(p_ast_declarator_t, &parser->ast_allocator);

  bool found_func_def = false;

  while (!p_check_cur_tok(parser, P_TOKEN_SEMICOLON) && !found_func_def) {
    p_ast_declarator_t declarator = {
        .type = ty,
    };

    // TODO: Handle type qualifiers
    while (p_expect_cur_tok(parser, P_TOKEN_STAR)) {
      p_ast_type_t ptr_ty = {
          .kind = P_TY_PTR,
          .u.ptr.base_ty = bump_alloc(&parser->ast_bump, sizeof(p_ast_type_t)),
      };
      *ptr_ty.u.ptr.base_ty = declarator.type;
      declarator.type = ptr_ty;
    }

    const char *ident = parser->cur_tok->ident_literal;
    if (!p_check_cur_tok(parser, P_TOKEN_IDENT)) {
      log_error("Expected name of decl or semicolon, received: %s",
                p_cur_tok_fmt(parser));
      return false;
    }

    p_next_token(parser);

    log_debug("cur tok: %s", p_cur_tok_fmt(parser));

    while (p_expect_cur_tok(parser, P_TOKEN_LSQUARE)) {
      log_debug("Array begin");
      p_ast_expr_t count_expr = {0};
      if (!p_expect_cur_tok(parser, P_TOKEN_RSQUARE)) {
        // TODO: Better optional expr parsing
        bool success = p_parse_expr(parser, &count_expr);
        p_next_token(parser);
        if (!p_expect_cur_tok(parser, P_TOKEN_RSQUARE)) {
          log_error(
              "Expected right square brackets after array count expression");
          return false;
        }
      }

      p_ast_type_t arr_ty = {
          .kind = P_TY_ARRAY,
          .u.array =
              {
                  .elem_ty =
                      bump_alloc(&parser->ast_bump, sizeof(p_ast_type_t)),
                  .count = bump_alloc(&parser->ast_bump, sizeof(p_ast_expr_t)),
              },
      };
      *arr_ty.u.array.elem_ty = declarator.type;
      *arr_ty.u.array.count = count_expr;
      declarator.type = arr_ty;
    }

    declarator.name = ident;

    switch (parser->cur_tok->kind) {
    case P_TOKEN_ASSIGN: {
      declarator.has_init = true;

      p_next_token(parser);

      bool init_success = p_parse_init(parser, &declarator.init);
      if (!init_success) {
        log_error("Failed to parse init for decl");
        return false;
      }

      if (!p_expect_peek_tok(parser, P_TOKEN_SEMICOLON) &&
          !p_expect_peek_tok(parser, P_TOKEN_COMMA)) {
        log_error("Expected semicolon or comma after decl init");
        return false;
      }

    } break;
    case P_TOKEN_LPAREN: {
      p_ast_type_t func_ty = {
          .kind = P_TY_FUNC,
          .u.func =
              {
                  .ret_type =
                      bump_alloc(&parser->ast_bump, sizeof(p_ast_type_t)),
                  .args = array_new(p_ast_field_t, &parser->ast_allocator),
              },
      };
      struct p_ast_type_func *func = &func_ty.u.func;
      *func->ret_type = ty;

      p_next_token(parser);

      if (!p_expect_cur_tok(parser, P_TOKEN_RPAREN)) {
        log_error("Args not supported yet");
        return false;
      }

      declarator.type = func_ty;

      switch (parser->cur_tok->kind) {
      case P_TOKEN_SEMICOLON: {
        log_debug("Found function decl");
      } break;
      case P_TOKEN_LCURLY: {
        log_debug("Found function def");
        found_func_def = true;
      } break;
      default: {

      } break;
      }
    } break;
    case P_TOKEN_COMMA: {
      p_next_token(parser);
    } break;
    case P_TOKEN_SEMICOLON: {
      log_debug("Parsed variable decl");
    } break;
    default: {
      log_error("Expected semicolon, assign or left parenthesis after "
                "declaration name, received token: %s",
                p_cur_tok_fmt(parser));
    } break;
    }

    log_debug("decl Type: %s",
              p_type_fmt(&declarator.type, &parser->ast_allocator).string);

    log_debug("Name: %s", declarator.name);

    array_add(declarators, declarator);
  }

  decl->decls = declarators;

  // TODO: Special stuff for function pointer types

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
