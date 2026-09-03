#pragma once

#include "ast.h"
#include "lexer.h"
#include "lilc/hashmap.h"

typedef Hashmap hashmap_t;

typedef struct {
  const char *filename;
  const char *source;
  const p_tok_t *tokens;

  hashmap_t typedef_names;

  p_ast_t ast;

  const p_tok_t *cur_tok;
  const p_tok_t *peek_tok;

  bump_t ast_bump;
  allocator_t ast_allocator;
} p_parser_t;

void p_parser_init(p_parser_t *parser);

void p_parser_parse(p_parser_t *parser, const p_tok_t *tokens,
                    const char *filename, const char *source);

void p_parser_deinit(p_parser_t *parser);
