/*
 * L Prefix in relation to operators means logic and B prefix means bitwise.
 * Example: LOR, LAND, BXOR, BAND
 */

#pragma once

#include "lilc/alloc.h"
#include "lilc/dynstr.h"
#include "lilc/numbers.h"
#include <stdbool.h>

typedef struct p_ast_type p_ast_type_t;
typedef struct p_ast_expr p_ast_expr_t;
typedef struct p_ast_stmt p_ast_stmt_t;
typedef struct p_ast_decl p_ast_decl_t;
typedef struct p_ast_init p_ast_init_t;

/* -- Types --*/

typedef enum {
  P_TY_VOID,
  P_TY_BOOL,

  P_TY_CHAR,
  P_TY_SCHAR,
  P_TY_UCHAR,
  P_TY_SHORT,
  P_TY_USHORT,
  P_TY_INT,
  P_TY_UINT,
  P_TY_LONG,
  P_TY_ULONG,
  P_TY_LLONG,
  P_TY_ULLONG,
  P_TY_FLOAT,
  P_TY_DOUBLE,
  P_TY_LDOUBLE,

  P_TY_PTR,
  P_TY_ARRAY,
  P_TY_FUNC,

  P_TY_STRUCT,
  P_TY_UNION,
  P_TY_ENUM,
  P_TY_TYPEDEF_NAME,
} p_type_kind_e;

typedef struct p_ast_field p_ast_field_t;

typedef struct p_ast_enum_item {
  const char *name;
  i64 val;
  bool has_val;
} p_ast_enum_item_t;

struct p_ast_type {
  p_type_kind_e kind;
  union p_ast_type_var {
    struct p_ast_type_ptr {
      p_ast_type_t *base_ty;
    } ptr;
    struct p_ast_type_array {
      p_ast_type_t *elem_ty;
      p_ast_expr_t *count;
      bool is_vla;
    } array;
    struct p_ast_type_func {
      p_ast_type_t *ret_type;
      p_ast_field_t *args;
      bool variadic;
    } func;
    struct p_ast_type_rec {
      const char *tag;
      p_ast_field_t *fields;
      bool is_complete;
    } rec; // struct/union
    struct p_ast_type_enum {
      const char *tag;
      p_ast_enum_item_t *items;
      bool is_complete;
    } en;
    const char *typedef_name;
  } u;
};

struct p_ast_field {
  char *name;
  p_ast_type_t ty;
};

/* -- Exprs --*/

typedef enum p_ast_expr_kind {
  P_EX_INT,
  P_EX_FLOAT,
  P_EX_STR,
  P_EX_CHAR,
  P_EX_IDENT,

  P_EX_UNARY,
  P_EX_BINARY,
  P_EX_TERNARY,
  P_EX_ASSIGN,
  P_EX_COMMA,

  P_EX_CALL,
  P_EX_INDEX,
  P_EX_MEMBER,     /* . */
  P_EX_PTR_MEMBER, /* -> */
  P_EX_POST_INC,
  P_EX_POST_DEC,
  P_EX_PRE_INC,
  P_EX_PRE_DEC,

  P_EX_CAST,
  P_EX_SIZEOF_EXPR,
  P_EX_SIZEOF_TYPE,

  P_EX_COMPOUND_LIT
} p_ast_expr_kind_e;

typedef enum p_unary_op {
  P_UOP_POS,
  P_UOP_NEG,
  P_UOP_BNOT,
  P_UOP_LNOT,
  P_UOP_ADDR,
  P_UOP_DEREF,
} p_unary_op_e;

typedef enum p_binary_op {
  P_BOP_MUL,
  P_BOP_DIV,
  P_BOP_MOD,
  P_BOP_ADD,
  P_BOP_SUB,

  P_BOP_SHL,
  P_BOP_SHR,

  P_BOP_LT,
  P_BOP_LTE,
  P_BOP_GT,
  P_BOP_GTE,

  P_BOP_EQ,
  P_BOP_NEQ,

  P_BOP_BAND,
  P_BOP_BXOR,
  P_BOP_BOR,

  P_LAND,
  P_LOR,
} p_binary_op_e;

typedef enum p_assign_op {
  P_AOP_ASSIGN,
  P_AOP_ADD_ASSIGN,
  P_AOP_SUB_ASSIGN,
  P_AOP_MUL_ASSIGN,
  P_AOP_DIV_ASSIGN,
  P_AOP_MOD_ASSIGN,
  P_AOP_SHL_ASSIGN,
  P_AOP_SHR_ASSIGN,
  P_AOP_BAND_ASSIGN,
  P_AOP_BXOR_ASSIGN,
  P_AOP_BOR_ASSIGN
} p_assign_op_e;

struct p_ast_expr {
  p_ast_expr_kind_e kind;
  union p_ast_expr_var {
    i64 integer;
    f64 fp;
    const char *str;
    char ch;
    const char *ident;

    struct p_ast_expr_unary {
      p_unary_op_e op;
      p_ast_expr_t *ex;
    } unary_op;
    struct p_ast_expr_binary {
      p_binary_op_e op;
      p_ast_expr_t *left;
      p_ast_expr_t *right;
    } binary_op;
    struct p_ast_expr_ternary {
      p_ast_expr_t *cond;
      p_ast_expr_t *true_case;
      p_ast_expr_t *false_case;
    } ternary_op;
    struct p_ast_expr_assign {
      p_assign_op_e op;
      p_ast_expr_t *left;
      p_ast_expr_t *right;
    } assign;
    struct p_ast_expr_comma {
      p_ast_expr_t *left;
      p_ast_expr_t *right;
    } comma;

    struct p_ast_expr_call {
      p_ast_expr_t *callee;
      p_ast_expr_t *args;
    } call;
    struct p_ast_expr_index {
      p_ast_expr_t *base;
      p_ast_expr_t *idx;
    } index;
    struct p_ast_expr_member {
      p_ast_expr_t *base;
      const char *member_field;
    } member;

    struct {
      p_ast_type_t ty;
      p_ast_expr_t *val;
    } cast;
    struct {
      p_ast_type_t ty;
      p_ast_init_t *init;
    } compound_lit;
    p_ast_expr_t *sizeof_expr;
    p_ast_type_t sizeof_type;
  } u;
};

/* -- Inits --*/

typedef enum p_ast_init_kind {
  P_INIT_EXPR, // Single expr like (int[]){0}
  P_INIT_LIST, // List of exprs like (int[]){[0] = 0, [1] = 1, 2}
} p_ast_init_kind_e;

typedef struct p_ast_init_desig {
  enum p_ast_init_desig_kind { P_INIT_DESIG_FIELD, P_INIT_DESIG_INDEX } kind;
  union p_ast_init_desig_var {
    const char *field;
    p_ast_expr_t *index;
  } u;
} p_ast_init_desig_t;

typedef struct p_ast_init_item {
  p_ast_init_desig_t *desigs;
  p_ast_init_t *value;
} p_ast_init_item_t;

struct p_ast_init {
  p_ast_init_kind_e kind;
  union p_ast_init_var {
    p_ast_expr_t *expr;
    p_ast_init_item_t *list;
  } u;
};

/* -- Decls --*/

typedef enum p_ast_storage_class {
  P_SC_NONE,
  P_SC_TYPEDEF,
  P_SC_EXTERN,
  P_SC_STATIC,
  P_SC_AUTO,
  P_SC_REGISTER,
} p_ast_storage_class_e;

typedef enum p_ast_type_qual {
  P_TQ_NONE = 0,
  P_TQ_CONST = 1 << 0,
  P_TQ_VOLATILE = 1 << 1,
  P_TQ_RESTRICT = 1 << 2,
  P_TQ_ATOMIC = 1 << 3
} p_ast_type_qual_e;

typedef struct p_ast_declarator {
  const char *name; // NULL for abstract declarator
  p_ast_type_t type;
  p_ast_init_t init; // optional
  bool has_init;
} p_ast_declarator_t;

struct p_ast_decl {
  p_ast_storage_class_e sc;
  u32 quals; // p_ast_type_qual_e bitmask
  p_ast_type_t base_type;
  p_ast_declarator_t *decls;
};

/* -- Stmts --*/

typedef enum p_ast_stmt_kind {
  P_ST_EMPTY,
  P_ST_EXPR,
  P_ST_COMPOUND, /* block */
  P_ST_IF,
  P_ST_SWITCH,
  P_ST_WHILE,
  P_ST_DO_WHILE,
  P_ST_FOR,
  P_ST_RETURN,
  P_ST_BREAK,
  P_ST_CONTINUE,
  P_ST_GOTO,
  P_ST_LABEL,
  P_ST_CASE,
  P_ST_DEFAULT,
  P_ST_DECL /* declaration as block item wrapper */
} p_ast_stmt_kind_e;

typedef struct p_ast_block_item {
  bool is_decl;
  union p_ast_block_item_var {
    p_ast_decl_t decl;
    p_ast_stmt_t *stmt;
  } u;
} p_ast_block_item_t;

struct p_ast_stmt {
  p_ast_stmt_kind_e kind;
  union p_ast_stmt_var {
    p_ast_expr_t expr;
    p_ast_block_item_t *compound_items;

    struct p_ast_stmt_if {
      p_ast_expr_t cond;
      p_ast_stmt_t *then_branch;
      p_ast_stmt_t *else_branch;
    } if_stmt;
    struct p_ast_stmt_while {
      p_ast_expr_t cond;
      p_ast_stmt_t *body;
    } while_stmt;
    struct p_ast_stmt_do_while {
      p_ast_stmt_t *body;
      p_ast_expr_t *cond;
    } do_while;
    struct p_ast_stmt_switch {
      p_ast_expr_t *cond;
      p_ast_stmt_t *body;
    } switch_stmt;
    struct p_ast_stmt_for {
      bool has_init_expr; // If true, use init_expr else use init_decl
      p_ast_expr_t init_expr;
      p_ast_decl_t init_decl;

      p_ast_expr_t cond;
      p_ast_expr_t post;
      p_ast_stmt_t *body;
    } for_stmt;

    struct p_ast_stmt_return {
      p_ast_expr_t expr;
      bool has_return_expr;
    } return_stmt;
    struct p_ast_stmt_label {
      const char *label;
      p_ast_stmt_t *stmt;
    } label;
    struct {
      p_ast_expr_t val;
      p_ast_stmt_t *stmt;
    } case_stmt;
    struct {
      p_ast_stmt_t *stmt;
    } default_stmt;
    struct {
      const char *label;
    } goto_stmt;
    struct {
      p_ast_decl_t decl;
    } decl_stmt;
  } u;
};

/* -- Top-Level --*/

typedef enum p_ast_ext_kind {
  P_EXT_DECL,
  P_EXT_FUNC_DEF,
} p_ast_ext_kind_e;

typedef struct p_ast_func_def {
  p_ast_decl_t decl; // Function declarator and specs
  p_ast_stmt_t body; // Compound statement
} p_ast_func_def_t;

typedef struct p_ast_extern {
  p_ast_ext_kind_e kind;
  union p_ast_extern_var {
    p_ast_decl_t decl;
    p_ast_func_def_t func;
  } u;
} p_ast_extern_t;

typedef struct {
  p_ast_extern_t *externs;
} p_ast_t;

dyn_string_t p_ast_fmt(const p_ast_t *ast, allocator_t *fmt_alloc);

dyn_string_t p_type_fmt(const p_ast_type_t *ty, allocator_t *fmt_alloc);

dyn_string_t p_expr_fmt(const p_ast_expr_t *expr, allocator_t *fmt_alloc);

dyn_string_t p_stmt_fmt(const p_ast_stmt_t *stmt, allocator_t *fmt_alloc);

dyn_string_t p_extern_fmt(const p_ast_extern_t *external,
                          allocator_t *fmt_alloc);

dyn_string_t p_init_fmt(const p_ast_init_t *init,
                          allocator_t *fmt_alloc);
