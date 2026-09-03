#pragma once

#include "lilc/alloc.h"
#include "lilc/numbers.h"
#include "lilc/dynstr.h"

typedef enum {
  P_TOKEN_INVALID,
  P_TOKEN_EOF,

  P_TOKEN_IDENT,
  P_TOKEN_INTEGER,
  P_TOKEN_FLOAT,
  P_TOKEN_STRING,
  P_TOKEN_CHAR,
  
  P_TOKEN_COMMA, // ','
  P_TOKEN_SEMICOLON, // ';'
  P_TOKEN_COLON, // ':'
  P_TOKEN_QUESTION, // '?'
  P_TOKEN_DOT, // '..'
  P_TOKEN_DOT_DOT_DOT, // '...'
  P_TOKEN_HASH, // '#'
  P_TOKEN_HASH_HASH, // '##'
  P_TOKEN_LPAREN, // '('
  P_TOKEN_RPAREN, // ')'
  P_TOKEN_LSQUARE, // '['
  P_TOKEN_RSQUARE, // ']'
  P_TOKEN_LCURLY, // '{'
  P_TOKEN_RCURLY, // '}'

  P_TOKEN_PLUS, // '+'
  P_TOKEN_MINUS, // '-'
  P_TOKEN_STAR, // '*'
  P_TOKEN_SLASH, // '/'
  P_TOKEN_PERCENT, // '%'
  P_TOKEN_AMP, // '&'
  P_TOKEN_PIPE, // '|'
  P_TOKEN_CARET, // '^'
  P_TOKEN_BANG, // '!'
  P_TOKEN_TILDE, // '~'
  P_TOKEN_ASSIGN, // '='
  P_TOKEN_LT, // '<'
  P_TOKEN_GT, // '>'
  P_TOKEN_PLUS_PLUS, // '++'
  P_TOKEN_MINUS_MINUS, // '--'
  P_TOKEN_ARROW, // '->'
  P_TOKEN_PLUS_ASSIGN, // '+='
  P_TOKEN_MINUS_ASSIGN, // '-='
  P_TOKEN_STAR_ASSIGN, // '*='
  P_TOKEN_SLASH_ASSIGN, // '/='
  P_TOKEN_PERCENT_ASSIGN, // '&='
  P_TOKEN_AMP_ASSIGN, // '&='
  P_TOKEN_PIPE_ASSIGN, // '|='
  P_TOKEN_CARET_ASSIGN, // '^='
  P_TOKEN_LT_LT, // '<<'
  P_TOKEN_GT_GT, // '>>'
  P_TOKEN_LT_LT_ASSIGN, // '<<='
  P_TOKEN_GT_GT_ASSIGN, // '<<='
  P_TOKEN_EQ_EQ, // '=='
  P_TOKEN_BANG_EQ, // '!='
  P_TOKEN_LT_EQ, // '<='
  P_TOKEN_GT_EQ, // '>='
  P_TOKEN_AMP_AMP, // '&&'
  P_TOKEN_PIPE_PIPE, // '||'
} p_tok_kind_e;

typedef enum {
  P_TOKEN_INT_DEC, // default
  P_TOKEN_INT_BIN, // 0b...
  P_TOKEN_INT_OCT, // 0o...
  P_TOKEN_INT_HEX, // 0x...
} p_tok_int_kind_e;

typedef struct {
  p_tok_kind_e kind;
  char *string_literal;
  char *ident_literal;
  char char_literal;
  i64 int_literal;
  p_tok_int_kind_e int_kind;
  f64 float_literal;

  u32 line;
  u32 line_pos;
  u32 length;
} p_tok_t;

typedef struct {
  const char *filename;
  const char *source;

  p_tok_t *tokens;

  const char *cur_ch;

  u64 pos;
  u64 line;

  bump_t tokens_bump;
  allocator_t tokens_alloc;
} p_lexer_t;

void p_lexer_init(p_lexer_t *lexer);

void p_lexer_deinit(p_lexer_t *lexer);

void p_lexer_tokenize(p_lexer_t *lexer, const char *source,
                      const char *filename);

dyn_string_t p_tok_fmt(const p_tok_t *tok, allocator_t *fmt_alloc);
