#include "../include/ast.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/dynstr.h"
#include <lilc/log.h>

dyn_string_t p_expr_fmt(const p_ast_expr_t *expr, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  dyn_string_printf(&str, "EXPR_EMPTY");

  switch (expr->kind) {
  case P_EX_INT: {
    dyn_string_printf(&str, "EXPR_INT(%ld)", expr->u.integer);
  } break;
  case P_EX_FLOAT: {
    dyn_string_printf(&str, "EXPR_FLOAT(%f)", expr->u.fp);
  } break;
  case P_EX_STR: {
    dyn_string_printf(&str, "EXPR_STRING(\"%s\")", expr->u.str);
  } break;
  case P_EX_CHAR: {
    dyn_string_printf(&str, "EXPR_CHAR('%c')", expr->u.ch);
  } break;
  case P_EX_IDENT: {
    dyn_string_printf(&str, "EXPR_IDENT(%s)", expr->u.ident);
  } break;
  case P_EX_UNARY: {

  } break;
  case P_EX_BINARY: {

  } break;
  case P_EX_TERNARY: {

  } break;
  case P_EX_ASSIGN: {

  } break;
  case P_EX_COMMA: {

  } break;
  case P_EX_CALL: {

  } break;
  case P_EX_INDEX: {

  } break;
  case P_EX_MEMBER: {

  } break;
  case P_EX_PTR_MEMBER: {

  } break;
  case P_EX_POST_INC: {

  } break;
  case P_EX_POST_DEC: {

  } break;
  case P_EX_PRE_INC: {

  } break;
  case P_EX_PRE_DEC: {

  } break;
  case P_EX_CAST: {

  } break;
  case P_EX_SIZEOF_EXPR: {

  } break;
  case P_EX_SIZEOF_TYPE: {

  } break;
  case P_EX_COMPOUND_LIT: {

  } break;
  }

  return str;
}

dyn_string_t p_arg_list_fmt(const p_ast_arg_t *args, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  if (args == NULL || array_len(args) == 0) {
    return str;
  }

  log_debug("ARGS COUNT: %zu", array_len(args));

  p_ast_arg_t *arg;
  array_foreach(args, arg) {
    if (arg->name != NULL) {
      dyn_string_add_char(&str, '(');
    }
    dyn_string_add_str(&str, p_type_fmt(&arg->ty, fmt_alloc).string);
    if (arg->name != NULL) {
      dyn_string_add_str(&str, ", ");
      dyn_string_add_str(&str, arg->name);
      dyn_string_add_char(&str, ')');
    }

    if (_arr_foreach_idx != array_len(args) - 1) {
      dyn_string_add_str(&str, ", ");
    }
  }

  return str;
}

dyn_string_t p_type_fmt(const p_ast_type_t *ty, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  switch (ty->kind) {
  case P_TY_VOID: {
    dyn_string_copy_str(&str, "TY_VOID");
  } break;
  case P_TY_BOOL: {
    dyn_string_copy_str(&str, "TY_BOOL");
  } break;
  case P_TY_CHAR: {
    dyn_string_copy_str(&str, "TY_CHAR");
  } break;
  case P_TY_SCHAR: {
    dyn_string_copy_str(&str, "TY_SIGNED_CHAR");
  } break;
  case P_TY_UCHAR: {
    dyn_string_copy_str(&str, "TY_UnSIGNED_CHAR");
  } break;
  case P_TY_SHORT: {
    dyn_string_copy_str(&str, "TY_SHORT");
  } break;
  case P_TY_USHORT: {
    dyn_string_copy_str(&str, "TY_UNSIGNED_SHORT");
  } break;
  case P_TY_INT: {
    dyn_string_copy_str(&str, "TY_INT");
  } break;
  case P_TY_UINT: {
    dyn_string_copy_str(&str, "TY_UNSIGNED_INT");
  } break;
  case P_TY_LONG: {
    dyn_string_copy_str(&str, "TY_LONG");
  } break;
  case P_TY_ULONG: {
    dyn_string_copy_str(&str, "TY_UNSIGNED_LONG");
  } break;
  case P_TY_LLONG: {
    dyn_string_copy_str(&str, "TY_LONG_LONG");
  } break;
  case P_TY_ULLONG: {
    dyn_string_copy_str(&str, "TY_UNSIGNED_LONG_LONG");
  } break;
  case P_TY_FLOAT: {
    dyn_string_copy_str(&str, "TY_FLOAT");
  } break;
  case P_TY_DOUBLE: {
    dyn_string_copy_str(&str, "TY_DOUBLE");
  } break;
  case P_TY_LDOUBLE: {
    dyn_string_copy_str(&str, "TY_LONG_DOUBLE");
  } break;
  case P_TY_PTR: {
    dyn_string_printf(&str, "TY_PTR(%s)",
                      p_type_fmt(ty->u.ptr.base_ty, fmt_alloc).string);
  } break;
  case P_TY_ARRAY: {
    dyn_string_printf(&str, "TY_ARRAY(%s",
                      p_type_fmt(ty->u.array.elem_ty, fmt_alloc).string);
    if (ty->u.array.has_count) {
      dyn_string_add_str(&str, ", ");
      dyn_string_add_str(&str, p_expr_fmt(ty->u.array.count, fmt_alloc).string);
    }
    dyn_string_add_char(&str, ')');
  } break;
  case P_TY_FUNC: {
    log_debug("Func type");
    struct p_ast_type_func func = ty->u.func;
    dyn_string_printf(&str, "TY_FUNC(%s, [%s])",
                      p_type_fmt(func.ret_type, fmt_alloc).string, p_arg_list_fmt(func.args, fmt_alloc).string);
  } break;
  case P_TY_STRUCT: {
    dyn_string_copy_str(&str, "TY_STRUCT");
  } break;
  case P_TY_UNION: {
    dyn_string_copy_str(&str, "TY_UNION");
  } break;
  case P_TY_ENUM: {
    dyn_string_copy_str(&str, "TY_ENUM");
  } break;
  case P_TY_TYPEDEF_NAME: {
    dyn_string_copy_str(&str, "TY_TYPEDEF");
  } break;
  }

  return str;
}

dyn_string_t p_desig_fmt(const p_ast_init_desig_t *desigs,
                         allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  if (array_len(desigs) > 1) {
    dyn_string_add_str(&str, "[");
  }

  p_ast_init_desig_t *desig;
  array_foreach(desigs, desig) {
    dyn_string_t desig_str;
    switch (desig->kind) {
    case P_INIT_DESIG_FIELD: {
      desig_str =
          dyn_string_makef(fmt_alloc, "DESIG_FIELD(%s)", desig->u.field);
    } break;
    case P_INIT_DESIG_INDEX: {
      desig_str =
          dyn_string_makef(fmt_alloc, "DESIG_INDEX(%s)",
                           p_expr_fmt(desig->u.index, fmt_alloc).string);
    } break;
    }

    dyn_string_add_str(&str, desig_str.string);
    if (_arr_foreach_idx != array_len(desigs) - 1) {
      dyn_string_add_str(&str, ", ");
    }
  }

  if (array_len(desigs) > 1) {
    dyn_string_add_str(&str, "]");
  }

  return str;
}

dyn_string_t p_init_fmt(const p_ast_init_t *init, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  switch (init->kind) {
  case P_INIT_EXPR: {
    dyn_string_copy_str(&str, p_expr_fmt(init->u.expr, fmt_alloc).string);
  } break;
  case P_INIT_LIST: {
    dyn_string_copy_str(&str, "INIT_LIST(");
    p_ast_init_item_t *init_item;
    array_foreach(init->u.list, init_item) {
      if (init_item->desigs != NULL && array_len(init_item->desigs) != 0) {
        dyn_string_add_str(&str,
                           p_desig_fmt(init_item->desigs, fmt_alloc).string);
        dyn_string_add_str(&str, " = ");
      }

      dyn_string_add_str(&str, p_init_fmt(init_item->value, fmt_alloc).string);
      if (_arr_foreach_idx != array_len(init->u.list) - 1) {
        dyn_string_add_str(&str, ", ");
      }
    }
    dyn_string_add_char(&str, ')');
  } break;
  }

  return str;
}

dyn_string_t p_declarators_fmt(const p_ast_declarator_t *decls, u64 indent,
                               allocator_t *fmt_alloc);

static dyn_string_t p_declarator_fmt(const p_ast_declarator_t *decl, u64 indent,
                                     allocator_t *fmt_alloc);

static dyn_string_t p_stmt_fmt_indent(const p_ast_stmt_t *stmt, u64 indent,
                                      allocator_t *fmt_alloc);

static dyn_string_t p_stmt_compound_fmt(const p_ast_block_item_t *block_items,
                                        u64 indent, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  dyn_string_t str_indent = {0};
  dyn_string_init(&str_indent, fmt_alloc);

  for (u64 i = 0; i < indent; i++) {
    dyn_string_add_char(&str_indent, ' ');
  }

  p_ast_block_item_t *block_item;
  array_foreach(block_items, block_item) {
    if (block_item->is_decl) {
      if (array_len(block_item->u.decl.decls) == 1) {
        dyn_string_printf(
            &str, "%s%s", str_indent.string,
            p_declarator_fmt(&block_item->u.decl.decls[0], 0, fmt_alloc)
                .string);
      } else {
        dyn_string_printf(
            &str, "%sDECL(\n%s%s)", str_indent.string,
            p_declarators_fmt(block_item->u.decl.decls, indent + 2, fmt_alloc)
                .string,
            str_indent.string);
      }
    } else {
      dyn_string_t str0 =
          p_stmt_fmt_indent(block_item->u.stmt, indent, fmt_alloc);
      dyn_string_add_str(&str, str0.string);
    }
    dyn_string_add_char(&str, '\n');
  }

  return str;
}

static dyn_string_t p_stmt_fmt_indent(const p_ast_stmt_t *stmt, u64 indent,
                                      allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  dyn_string_t str_indent = {0};
  dyn_string_init(&str_indent, fmt_alloc);

  for (u64 i = 0; i < indent; i++) {
    dyn_string_add_char(&str_indent, ' ');
  }

  switch (stmt->kind) {
  case P_ST_EMPTY: {
    dyn_string_printf(&str, "%sSTMT_EMPTY", str_indent.string);
  } break;
  case P_ST_EXPR: {
    dyn_string_printf(&str, "%sSTMT_EXPR(%s)", str_indent.string,
                      p_expr_fmt(&stmt->u.expr, fmt_alloc).string);
  } break;
  case P_ST_COMPOUND: {
    dyn_string_printf(
        &str, "%sSTMT_COMPOUND(\n%s%s)", str_indent.string,
        p_stmt_compound_fmt(stmt->u.compound_items, indent + 2, fmt_alloc)
            .string,
        str_indent.string);
  } break;
  case P_ST_IF: {
  } break;
  case P_ST_SWITCH: {
  } break;
  case P_ST_WHILE: {
  } break;
  case P_ST_DO_WHILE: {
  } break;
  case P_ST_FOR: {
  } break;
  case P_ST_RETURN: {
    struct p_ast_stmt_return stmt_ret = stmt->u.return_stmt;
    dyn_string_printf(
        &str, "%sSTMT_RETURN%s", str_indent.string,
        stmt_ret.has_return_expr
            ? dyn_string_makef(fmt_alloc, "(%s)",
                               p_expr_fmt(&stmt_ret.expr, fmt_alloc).string)
                  .string
            : "");
  } break;
  case P_ST_BREAK: {
    dyn_string_printf(&str, "%sSTMT_BREAK", str_indent.string);
  } break;
  case P_ST_CONTINUE: {
    dyn_string_printf(&str, "%sSTMT_CONTINUE", str_indent.string);
  } break;
  case P_ST_GOTO: {
    dyn_string_printf(&str, "%sSTMT_GOTO(%s)", str_indent.string,
                      stmt->u.goto_stmt.label);
  } break;
  case P_ST_LABEL: {
  } break;
  case P_ST_CASE: {
  } break;
  case P_ST_DEFAULT: {
  } break;
  case P_ST_DECL: {
    if (array_len(stmt->u.decl_stmt.decl.decls) == 1) {
      dyn_string_printf(
          &str, "%sSTMT_%s", str_indent.string,
          p_declarator_fmt(&stmt->u.decl_stmt.decl.decls[0], 0, fmt_alloc)
              .string);
    } else {
      dyn_string_printf(&str, "DECL_NYI");
    }
  } break;
  }

  return str;
}

dyn_string_t p_stmt_fmt(const p_ast_stmt_t *stmt, allocator_t *fmt_alloc) {
  return p_stmt_fmt_indent(stmt, 0, fmt_alloc);
}

static dyn_string_t p_declarator_fmt(const p_ast_declarator_t *decl, u64 indent,
                                     allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  dyn_string_t str_indent = {0};
  dyn_string_init(&str_indent, fmt_alloc);

  for (u64 i = 0; i < indent; i++) {
    dyn_string_add_char(&str_indent, ' ');
  }

  dyn_string_printf(&str, "%sDECL(%s, %s", str_indent.string,
                    p_type_fmt(&decl->type, fmt_alloc).string, decl->name);
  if (decl->has_init) {
    dyn_string_add_str(&str, " = ");
    dyn_string_add_str(&str, p_init_fmt(&decl->init, fmt_alloc).string);
  }

  dyn_string_add_char(&str, ')');

  return str;
}

dyn_string_t p_decl_fmt(const p_ast_decl_t *decl, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  if (array_len(decl->decls) == 0) {
    dyn_string_printf(&str, "DECL(%s)",
                      p_type_fmt(&decl->base_type, fmt_alloc).string);
  } else if (array_len(decl->decls) == 1) {
    dyn_string_add_str(&str,
                       p_declarator_fmt(&decl->decls[0], 0, fmt_alloc).string);
  } else {
    dyn_string_printf(&str, "DECL(%s)[\n%s]",
                      p_type_fmt(&decl->base_type, fmt_alloc).string,
                      p_declarators_fmt(decl->decls, 2, fmt_alloc).string);
  }

  return str;
}

dyn_string_t p_declarators_fmt(const p_ast_declarator_t *decls, u64 indent,
                               allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  p_ast_declarator_t *decl;
  array_foreach(decls, decl) {
    dyn_string_add_str(&str, p_declarator_fmt(decl, indent, fmt_alloc).string);
    dyn_string_add_char(&str, '\n');
  }

  return str;
}

dyn_string_t p_extern_fmt(const p_ast_extern_t *external,
                          allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  switch (external->kind) {
  case P_EXT_DECL: {
    const p_ast_decl_t *decl = &external->u.decl;

    if (decl->decls == NULL || array_len(decl->decls) == 0) {
      dyn_string_printf(&str, "EXTERN_DECL(%s)",
                        p_type_fmt(&decl->base_type, fmt_alloc).string);
    } else if (array_len(decl->decls) == 1) {
      dyn_string_printf(&str, "EXTERN_%s",
                        p_declarator_fmt(&decl->decls[0], 0, fmt_alloc).string);
    } else {
      log_debug("multi ext decl: %zu", array_len(decl->decls));
      dyn_string_printf(&str, "EXTERN_DECL(%s)[\n%s]",
                        p_type_fmt(&decl->base_type, fmt_alloc).string,
                        p_declarators_fmt(decl->decls, 2, fmt_alloc).string);
    }

  } break;
  case P_EXT_FUNC_DEF: {
    const p_ast_func_def_t *func_def = &external->u.func;

    dyn_string_printf(
        &str, "EXTERN_FUNC_DEF(%s, BLOCK(\n%s))",
        p_declarator_fmt(&func_def->decl.decls[0], 0, fmt_alloc).string,
        p_stmt_compound_fmt(func_def->body.u.compound_items, 2, fmt_alloc)
            .string);
  } break;
  }

  return str;
}

dyn_string_t p_ast_fmt(const p_ast_t *ast, allocator_t *fmt_alloc) {
  dyn_string_t str = {0};
  dyn_string_init(&str, fmt_alloc);

  p_ast_extern_t *external;
  array_foreach(ast->externs, external) {
    dyn_string_t extern_str = p_extern_fmt(external, fmt_alloc);
    dyn_string_add_str(&str, extern_str.string);
    if (_arr_foreach_idx != array_len(ast->externs) - 1) {
      dyn_string_add_char(&str, '\n');
    }
  }

  return str;
}
