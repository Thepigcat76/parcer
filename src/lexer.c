#include "../include/lexer.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/dynstr.h"
#include "lilc/log.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

void p_lexer_init(p_lexer_t *lexer) {
  bump_init(&lexer->tokens_bump, 4096);
  bump_allocator_init(&lexer->tokens_alloc, &lexer->tokens_bump);
}

void p_lexer_deinit(p_lexer_t *lexer) { bump_free(&lexer->tokens_bump); }

static bool p_next_char(p_lexer_t *lexer) {
  if (*lexer->cur_ch == '\n') {
    lexer->pos = 1;
    lexer->line++;
  } else {
    lexer->pos++;
  }
  lexer->cur_ch++;
  return *lexer->cur_ch != '\0';
}

static bool p_valid_int_char(char ch, p_tok_int_kind_e int_kind) {
  switch (int_kind) {
  case P_TOKEN_INT_DEC:
    return isdigit(ch);
  case P_TOKEN_INT_BIN:
    return ch == '0' || ch == '1';
  case P_TOKEN_INT_OCT:
    return ch >= '0' && ch < '8';
  case P_TOKEN_INT_HEX:
    return isxdigit(ch);
  }
  return false;
}

static bool p_valid_ident_char(char ch, bool begin) {
  if (begin) {
    return isalpha(ch) || ch == '_' || ch == '$';
  }
  return isalnum(ch) || ch == '_' || ch == '$';
}

static void lexer_end(p_lexer_t *lexer, p_tok_t *cur_tok) {
  if (cur_tok != NULL) {
    array_add(lexer->tokens, *cur_tok);
  }
  p_tok_t eof_tok = {
      .kind = P_TOKEN_EOF,
      .line = lexer->line,
      .line_pos = lexer->pos,
      .length = 1,
  };
  array_add(lexer->tokens, eof_tok);
}

void p_lexer_tokenize(p_lexer_t *lexer, const char *source,
                      const char *filename) {
  lexer->filename = filename;
  lexer->source = source;

  bump_reset(&lexer->tokens_bump);
  lexer->tokens = array_new(p_tok_t, &lexer->tokens_alloc);

  lexer->cur_ch = source;
  // empty source file
  if (*lexer->cur_ch == '\0') {
    lexer_end(lexer, NULL);
    return;
  }

  for (;;) {
    p_tok_t tok = {
        .kind = P_TOKEN_INVALID,
        .line = lexer->line,
        .line_pos = lexer->pos,
        .length = 1,
    };

    if (*lexer->cur_ch == '\0') {
      lexer_end(lexer, NULL);
      return;
    }

    while (isspace(*lexer->cur_ch) || *lexer->cur_ch == '\n') {
      if (!p_next_char(lexer)) {
        lexer_end(lexer, NULL);
        return;
      }
    }

    switch (*lexer->cur_ch) {
    case ',': {
      tok.kind = P_TOKEN_COMMA;
    } break;
    case ';': {
      tok.kind = P_TOKEN_SEMICOLON;
    } break;
    case ':': {
      tok.kind = P_TOKEN_COLON;
    } break;
    case '?': {
      tok.kind = P_TOKEN_QUESTION;
    } break;
    case '.': {
      tok.kind = P_TOKEN_DOT;
      if (strncmp(lexer->cur_ch, "...", 3) == 0) {
        tok.kind = P_TOKEN_DOT_DOT_DOT;
        tok.length = 3;
        p_next_char(lexer);
        p_next_char(lexer);
      }
    } break;
    case '#': {
      tok.kind = P_TOKEN_HASH;
      if (strncmp(lexer->cur_ch, "##", 2) == 0) {
        tok.kind = P_TOKEN_HASH_HASH;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '(': {
      tok.kind = P_TOKEN_LPAREN;
    } break;
    case ')': {
      tok.kind = P_TOKEN_RPAREN;
    } break;
    case '[': {
      tok.kind = P_TOKEN_LSQUARE;
    } break;
    case ']': {
      tok.kind = P_TOKEN_RSQUARE;
    } break;
    case '{': {
      tok.kind = P_TOKEN_LCURLY;
    } break;
    case '}': {
      tok.kind = P_TOKEN_RCURLY;
    } break;
    case '+': {
      tok.kind = P_TOKEN_PLUS;
      if (strncmp(lexer->cur_ch, "++", 2) == 0) {
        tok.kind = P_TOKEN_PLUS_PLUS;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "+=", 2) == 0) {
        tok.kind = P_TOKEN_PLUS_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '-': {
      tok.kind = P_TOKEN_MINUS;
      if (strncmp(lexer->cur_ch, "--", 2) == 0) {
        tok.kind = P_TOKEN_MINUS_MINUS;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "-=", 2) == 0) {
        tok.kind = P_TOKEN_MINUS_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '*': {
      tok.kind = P_TOKEN_STAR;
      if (strncmp(lexer->cur_ch, "*=", 2) == 0) {
        tok.kind = P_TOKEN_STAR_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    // TODO: Comments
    case '/': {
      tok.kind = P_TOKEN_SLASH;
      if (strncmp(lexer->cur_ch, "/=", 2) == 0) {
        tok.kind = P_TOKEN_SLASH_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '%': {
      tok.kind = P_TOKEN_PERCENT;
      if (strncmp(lexer->cur_ch, "%=", 2) == 0) {
        tok.kind = P_TOKEN_PERCENT_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '&': {
      tok.kind = P_TOKEN_AMP;
      if (strncmp(lexer->cur_ch, "&&", 2) == 0) {
        tok.kind = P_TOKEN_AMP_AMP;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "&=", 2) == 0) {
        tok.kind = P_TOKEN_AMP_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '|': {
      tok.kind = P_TOKEN_PIPE;
      if (strncmp(lexer->cur_ch, "&&", 2) == 0) {
        tok.kind = P_TOKEN_PIPE_PIPE;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "&=", 2) == 0) {
        tok.kind = P_TOKEN_PIPE_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '^': {
      tok.kind = P_TOKEN_CARET;
      if (strncmp(lexer->cur_ch, "^=", 2) == 0) {
        tok.kind = P_TOKEN_CARET_ASSIGN;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '!': {
      tok.kind = P_TOKEN_BANG;
      if (strncmp(lexer->cur_ch, "!=", 2) == 0) {
        tok.kind = P_TOKEN_BANG_EQ;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '~': {
      tok.kind = P_TOKEN_TILDE;
    } break;
    case '=': {
      tok.kind = P_TOKEN_ASSIGN;
      if (strncmp(lexer->cur_ch, "==", 2) == 0) {
        tok.kind = P_TOKEN_EQ_EQ;
        tok.length = 2;
        p_next_char(lexer);
      }
    } break;
    case '<': {
      tok.kind = P_TOKEN_LT;
      if (strncmp(lexer->cur_ch, "<=", 2) == 0) {
        tok.kind = P_TOKEN_LT_EQ;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "<<", 2) == 0) {
        tok.kind = P_TOKEN_LT_LT;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "<<=", 3) == 0) {
        tok.kind = P_TOKEN_LT_LT_ASSIGN;
        tok.length = 3;
        p_next_char(lexer);
        p_next_char(lexer);
      }
    } break;
    case '>': {
      tok.kind = P_TOKEN_GT;
      if (strncmp(lexer->cur_ch, ">=", 2) == 0) {
        tok.kind = P_TOKEN_GT_EQ;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, "<<", 2) == 0) {
        tok.kind = P_TOKEN_GT_GT;
        tok.length = 2;
        p_next_char(lexer);
      } else if (strncmp(lexer->cur_ch, ">>=", 3) == 0) {
        tok.kind = P_TOKEN_GT_GT_ASSIGN;
        tok.length = 3;
        p_next_char(lexer);
        p_next_char(lexer);
      }
    } break;
    case '\'': {
      log_debug("tokenizing char");
      tok.kind = P_TOKEN_CHAR;
      p_next_char(lexer);
      tok.char_literal = *lexer->cur_ch;
      p_next_char(lexer);
      bool too_large = false;
      u64 len = 1;
      while (*lexer->cur_ch != '\'') {
        p_next_char(lexer);
        len++;
      }
      if (len > 1) {
        log_error("[LEXER] Char too long");
      }
      tok.length = len;
    } break;
    case '"': {
      tok.kind = P_TOKEN_STRING;
      p_next_char(lexer);
      dyn_string_t str_lit = {0};
      dyn_string_init(&str_lit, &lexer->tokens_alloc);
      while (*lexer->cur_ch != '"') {
        dyn_string_add_char(&str_lit, *lexer->cur_ch);
        if (!p_next_char(lexer)) {
          return;
        }
      }
      tok.string_literal = str_lit.string;
      tok.length = str_lit.len + 2;
    } break;
    default: {
      // TODO: Float parsing
      if (isdigit(*lexer->cur_ch)) {
        tok.kind = P_TOKEN_INTEGER;
        tok.int_kind = P_TOKEN_INT_DEC;
        u64 prefix_len = 0;
        if (*lexer->cur_ch == '0') {
          tok.int_kind = P_TOKEN_INT_OCT;
          prefix_len = 1;
          p_next_char(lexer);
          if (tolower(*lexer->cur_ch) == 'b') {
            prefix_len = 2;
            tok.int_kind = P_TOKEN_INT_BIN;
            p_next_char(lexer);
          } else if (tolower(*lexer->cur_ch) == 'o') {
            prefix_len = 2;
            tok.int_kind = P_TOKEN_INT_OCT;
            p_next_char(lexer);
          } else if (tolower(*lexer->cur_ch) == 'x') {
            prefix_len = 2;
            tok.int_kind = P_TOKEN_INT_HEX;
            p_next_char(lexer);
          }
        }

        // cur char is first digit of integer
        dyn_string_t int_lit = {0};
        dyn_string_init(&int_lit, &lexer->tokens_alloc);

        while (p_valid_int_char(*lexer->cur_ch, tok.int_kind)) {
          dyn_string_add_char(&int_lit, *lexer->cur_ch);
          if (p_valid_int_char(lexer->cur_ch[1], tok.int_kind)) {
            p_next_char(lexer);
          } else {
            break;
          }
        }

        i32 base = 10;
        switch (tok.int_kind) {
        case P_TOKEN_INT_DEC: {
          base = 10;
        } break;
        case P_TOKEN_INT_BIN: {
          base = 2;
        } break;
        case P_TOKEN_INT_OCT: {
          base = 8;
        } break;
        case P_TOKEN_INT_HEX: {
          base = 16;
        } break;
        }

        errno = 0;
        tok.int_literal = strtol(int_lit.string, NULL, base);
        if (errno == ERANGE) {
          log_error("[LEXER] Integer %s to large", int_lit.string);
        }
        tok.length = int_lit.len + prefix_len;
      } else if (p_valid_ident_char(*lexer->cur_ch, true)) {
        tok.kind = P_TOKEN_IDENT;
        dyn_string_t ident_lit = {0};
        dyn_string_init(&ident_lit, &lexer->tokens_alloc);

        while (p_valid_ident_char(*lexer->cur_ch, false)) {
          dyn_string_add_char(&ident_lit, *lexer->cur_ch);
          if (p_valid_ident_char(lexer->cur_ch[1], false)) {
            p_next_char(lexer);
          } else {
            break;
          }
        }

        tok.ident_literal = ident_lit.string;
        tok.length = ident_lit.len;
      }
    } break;
    }

    array_add(lexer->tokens, tok);

    p_next_char(lexer);
  }
}

dyn_string_t p_tok_fmt(const p_tok_t *tok, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  switch (tok->kind) {
  case P_TOKEN_INVALID: {
    dyn_string_copy_str(&str, "TOK_INVALID");
  } break;
  case P_TOKEN_EOF: {
    dyn_string_copy_str(&str, "TOK_EOF('\\0')");
  } break;
  case P_TOKEN_IDENT: {
    dyn_string_printf(&str, "TOK_IDENT(%s)", tok->ident_literal);
  } break;
  case P_TOKEN_INTEGER: {
    char *int_kind_str = "DEC";
    switch (tok->int_kind) {
    case P_TOKEN_INT_DEC:
      int_kind_str = "DEC";
      break;
    case P_TOKEN_INT_BIN:
      int_kind_str = "BIN";
      break;
    case P_TOKEN_INT_OCT:
      int_kind_str = "OCT";
      break;
    case P_TOKEN_INT_HEX:
      int_kind_str = "HEX";
      break;
    }
    dyn_string_printf(&str, "TOK_INTEGER(%ld,%s)", tok->int_literal,
                      int_kind_str);
  } break;
  case P_TOKEN_FLOAT: {
    dyn_string_printf(&str, "TOK_FLOAT(%f)", tok->float_literal);
  } break;
  case P_TOKEN_STRING: {
    dyn_string_printf(&str, "TOK_STRING(\"%s\")", tok->string_literal);
  } break;
  case P_TOKEN_CHAR: {
    dyn_string_printf(&str, "TOK_CHAR('%c')", tok->char_literal);
  } break;
  case P_TOKEN_COMMA: {
    dyn_string_copy_str(&str, "TOK_COMMA(',')");
  } break;
  case P_TOKEN_SEMICOLON: {
    dyn_string_copy_str(&str, "TOK_SEMICOLON(';')");
  } break;
  case P_TOKEN_COLON: {
    dyn_string_copy_str(&str, "TOK_COLON(':')");
  } break;
  case P_TOKEN_QUESTION: {
    dyn_string_copy_str(&str, "TOK_QUESTION('?')");
  } break;
  case P_TOKEN_DOT: {
    dyn_string_copy_str(&str, "TOK_DOT('.')");
  } break;
  case P_TOKEN_DOT_DOT_DOT: {
    dyn_string_copy_str(&str, "TOK_DOT_DOT_DOT('...')");
  } break;
  case P_TOKEN_HASH: {
    dyn_string_copy_str(&str, "TOK_HASH('#')");
  } break;
  case P_TOKEN_HASH_HASH: {
    dyn_string_copy_str(&str, "TOK_HASH_HASH('##')");
  } break;
  case P_TOKEN_LPAREN: {
    dyn_string_copy_str(&str, "TOK_LPAREN('(')");
  } break;
  case P_TOKEN_RPAREN: {
    dyn_string_copy_str(&str, "TOK_RPAREN(')')");
  } break;
  case P_TOKEN_LSQUARE: {
    dyn_string_copy_str(&str, "TOK_LSQUARE('[')");
  } break;
  case P_TOKEN_RSQUARE: {
    dyn_string_copy_str(&str, "TOK_RSQUARE(']')");
  } break;
  case P_TOKEN_LCURLY: {
    dyn_string_copy_str(&str, "TOK_LCURLY('{')");
  } break;
  case P_TOKEN_RCURLY: {
    dyn_string_copy_str(&str, "TOK_RCURLY('}')");
  } break;
  case P_TOKEN_PLUS: {
    dyn_string_copy_str(&str, "TOK_PLUS('+')");
  } break;
  case P_TOKEN_MINUS: {
    dyn_string_copy_str(&str, "TOK_MINUS('-')");
  } break;
  case P_TOKEN_STAR: {
    dyn_string_copy_str(&str, "TOK_STAR('*')");
  } break;
  case P_TOKEN_SLASH: {
    dyn_string_copy_str(&str, "TOK_SLASH('/')");
  } break;
  case P_TOKEN_PERCENT: {
    dyn_string_copy_str(&str, "TOK_PERCENT('%')");
  } break;
  case P_TOKEN_AMP: {
    dyn_string_copy_str(&str, "TOK_AMP('&')");
  } break;
  case P_TOKEN_PIPE: {
    dyn_string_copy_str(&str, "TOK_PIPE('|')");
  } break;
  case P_TOKEN_CARET: {
    dyn_string_copy_str(&str, "TOK_CARET('^')");
  } break;
  case P_TOKEN_BANG: {
    dyn_string_copy_str(&str, "TOK_BANG('!')");
  } break;
  case P_TOKEN_TILDE: {
    dyn_string_copy_str(&str, "TOK_TILDE('~')");
  } break;
  case P_TOKEN_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_ASSIGN('=')");
  } break;
  case P_TOKEN_LT: {
    dyn_string_copy_str(&str, "TOK_LT('<')");
  } break;
  case P_TOKEN_GT: {
    dyn_string_copy_str(&str, "TOK_GT('>')");
  } break;
  case P_TOKEN_PLUS_PLUS: {
    dyn_string_copy_str(&str, "TOK_PLUS_PLUS('++')");
  } break;
  case P_TOKEN_MINUS_MINUS: {
    dyn_string_copy_str(&str, "TOK_MINUS_MINUS('--')");
  } break;
  case P_TOKEN_ARROW: {
    dyn_string_copy_str(&str, "TOK_ARROW('->')");
  } break;
  case P_TOKEN_PLUS_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_PLUS_ASSIGN('+=')");
  } break;
  case P_TOKEN_MINUS_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_MINUS_ASSIGN('-=')");
  } break;
  case P_TOKEN_STAR_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_STAR_ASSIGN('*=')");
  } break;
  case P_TOKEN_SLASH_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_SLASH_ASSIGN('/=')");
  } break;
  case P_TOKEN_PERCENT_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_PERCENT_ASSIGN('%=')");
  } break;
  case P_TOKEN_AMP_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_AMP_ASSIGN('&=')");
  } break;
  case P_TOKEN_PIPE_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_PIPE_ASSIGN('|=')");
  } break;
  case P_TOKEN_CARET_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_CARET_ASSIGN('^=')");
  } break;
  case P_TOKEN_LT_LT: {
    dyn_string_copy_str(&str, "TOK_LT_LT('<<')");
  } break;
  case P_TOKEN_GT_GT: {
    dyn_string_copy_str(&str, "TOK_GT_GT('>>')");
  } break;
  case P_TOKEN_LT_LT_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_LT_LT_ASSIGN('<<=')");
  } break;
  case P_TOKEN_GT_GT_ASSIGN: {
    dyn_string_copy_str(&str, "TOK_GT_GT_ASSIGN('>>=')");
  } break;
  case P_TOKEN_EQ_EQ: {
    dyn_string_copy_str(&str, "TOK_EQ_EQ('==')");
  } break;
  case P_TOKEN_BANG_EQ: {
    dyn_string_copy_str(&str, "TOK_BANG_EQ('!=')");
  } break;
  case P_TOKEN_LT_EQ: {
    dyn_string_copy_str(&str, "TOK_LT_EQ('<=')");
  } break;
  case P_TOKEN_GT_EQ: {
    dyn_string_copy_str(&str, "TOK_GT_EQ('>=')");
  } break;
  case P_TOKEN_AMP_AMP: {
    dyn_string_copy_str(&str, "TOK_AMP_AMP('&&')");
  } break;
  case P_TOKEN_PIPE_PIPE: {
    dyn_string_copy_str(&str, "TOK_PIPE_PIPE('||')");
  } break;
  }

  return str;
}
