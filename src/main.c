#include "../include/lexer.h"
#include "../include/parser.h"
#include "lilc/array.h"
#include "lilc/file.h"
#include "lilc/log.h"
#include <lilc/alloc.h>
#include <lilc/dynstr.h>
#include <stdio.h>

#define EXAMPLE_FILEPATH "examples/decl_test.c"

int main(i32 argc, char **argv) {
  bump_t fmt_bump = {0};
  bump_init(&fmt_bump, 8192);
  allocator_t fmt_alloc = {0};
  bump_allocator_init(&fmt_alloc, &fmt_bump);

  p_lexer_t lexer = {0};
  p_lexer_init(&lexer);

  dyn_string_t source = file_read_to_string(EXAMPLE_FILEPATH, &HEAP_ALLOCATOR);
  if (source.string == NULL) {
    log_error("Failed to open file for testing parser");
    return 0;
  }

  log_info("[LEXER] Start tokenizing " EXAMPLE_FILEPATH);

  p_lexer_tokenize(&lexer, source.string, EXAMPLE_FILEPATH);

  log_info("[LEXER] Tokens:");

  p_tok_t *tok;
  array_foreach(lexer.tokens, tok) {
    dyn_string_t tok_str = p_tok_fmt(tok, &fmt_alloc);
    log_debug("Token: %s", tok_str.string);

    if (tok->kind == P_TOKEN_EOF) {
      break;
    }
  }

  p_parser_t parser = {0};
  p_parser_init(&parser);

  p_parser_parse(&parser, lexer.tokens, EXAMPLE_FILEPATH, source.string);

  dyn_string_t ast_str = p_ast_fmt(&parser.ast, &fmt_alloc);

  puts("AST:");
  puts(ast_str.string);

  p_parser_deinit(&parser);

  dyn_string_free(&source);

  p_lexer_deinit(&lexer);

  bump_free(&fmt_bump);
}
