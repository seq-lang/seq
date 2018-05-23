#ifndef SEQ_GRAMMAR_H
#define SEQ_GRAMMAR_H

#include <tao/pegtl.hpp>

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

/*
 * General language
 */
struct short_comment : pegtl::until<pegtl::eolf> {};
struct comment : pegtl::disable<pegtl::one<'#'>, short_comment> {};

struct sep : pegtl::sor<pegtl::ascii::space, comment> {};
struct seps : pegtl::star<sep> {};

struct str_let : TAO_PEGTL_STRING("let") {};
struct str_var : TAO_PEGTL_STRING("var") {};
struct str_end : TAO_PEGTL_STRING("end") {};
struct str_fun : TAO_PEGTL_STRING("fun") {};
struct str_if : TAO_PEGTL_STRING("if") {};
struct str_then : TAO_PEGTL_STRING("then") {};
struct str_elif : TAO_PEGTL_STRING("elif") {};
struct str_else : TAO_PEGTL_STRING("else") {};
struct str_source : TAO_PEGTL_STRING("source") {};
struct str_true : TAO_PEGTL_STRING("true") {};
struct str_false : TAO_PEGTL_STRING("false") {};

struct str_keyword : pegtl::sor<str_let, str_var, str_end, str_fun, str_if, str_then, str_elif, str_else, str_source, str_true, str_false> {};

struct name : pegtl::seq<pegtl::not_at<str_keyword>, pegtl::identifier> {};

struct odigit : pegtl::range<'0','7'> {};

struct pos_int_hex : pegtl::seq<pegtl::opt<pegtl::one<'+'>>, pegtl::string<'0','x'>, pegtl::plus<pegtl::xdigit>> {};
struct neg_int_hex : pegtl::seq<pegtl::one<'-'>, pegtl::string<'0','x'>, pegtl::plus<pegtl::xdigit>> {};

struct pos_int_dec : pegtl::seq<pegtl::opt<pegtl::one<'+'>>, pegtl::plus<pegtl::digit>> {};
struct neg_int_dec : pegtl::seq<pegtl::one<'-'>, pegtl::plus<pegtl::digit>> {};

struct pos_int_oct : pegtl::seq<pegtl::opt<pegtl::one<'+'>>, pegtl::one<'0'>, pegtl::plus<odigit>> {};
struct neg_int_oct : pegtl::seq<pegtl::one<'-'>, pegtl::one<'0'>, pegtl::plus<odigit>> {};

struct pos_int : pegtl::sor<pos_int_oct, pos_int_hex, pos_int_dec> {};
struct neg_int : pegtl::sor<neg_int_oct, neg_int_hex, neg_int_dec> {};
struct integer : pegtl::sor<pos_int, neg_int> {};

struct plus_minus : pegtl::opt<pegtl::one<'+','-'>> {};
struct dot : pegtl::one<'.'> {};
struct inf : pegtl::istring<'i','n','f'> {};
struct nan : pegtl::istring<'n','a','n'> {};

template< typename D >
struct number : pegtl::if_then_else<dot,
                                    pegtl::plus<D>,
                                    pegtl::seq<pegtl::plus<D>, dot, pegtl::star<D>>> {};

struct e : pegtl::one<'e','E'> {};
struct p : pegtl::one<'p','P'> {};
struct exponent : pegtl::seq<plus_minus, pegtl::plus<pegtl::digit>> {};
struct decimal : pegtl::seq<number<pegtl::digit>, pegtl::opt<e, exponent>> {};
struct hexadecimal : pegtl::seq<pegtl::one<'0'>, pegtl::one<'x','X'>, number<pegtl::xdigit>, pegtl::opt<p, exponent>> {};
struct numeral : pegtl::seq<plus_minus, pegtl::sor<hexadecimal, decimal, inf, nan>> {};

struct escape_chars : pegtl::one<'a', 'b', 'f', 'n', 'r', 't', 'v', '\\', '"'> {};
struct escaped : pegtl::seq<pegtl::one<'\\'>, escape_chars> {};
struct regular : pegtl::not_one<'\r', '\n'> {};
struct character : pegtl::sor<escaped, regular> {};

template<char Q>
struct short_string : pegtl::seq<pegtl::one<Q>, pegtl::until<pegtl::one<Q>, character>> {};
struct literal_string : short_string<'"'> {};

/*
 * Types
 */
struct type;
struct type_non_void;
struct void_type : TAO_PEGTL_STRING("Void") {};
struct seq_type : TAO_PEGTL_STRING("Seq") {};
struct int_type : TAO_PEGTL_STRING("Int") {};
struct float_type : TAO_PEGTL_STRING("Float") {};
struct bool_type : TAO_PEGTL_STRING("Bool") {};
struct record_type : pegtl::seq<pegtl::one<'{'>, seps, pegtl::list<type_non_void, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<'}'>> {};

struct type_2 : pegtl::sor<void_type, seq_type, int_type, float_type, bool_type, record_type> {};
struct array_component : pegtl::seq<pegtl::one<'['>, seps, pegtl::one<']'>, seps, pegtl::opt<array_component>> {};
struct array_type : pegtl::seq<type_2, seps, array_component> {};

struct type_non_void : pegtl::sor<record_type, array_type, seq_type, int_type, float_type, bool_type> {};
struct type : pegtl::sor<type_non_void, void_type> {};

/*
 * Expressions
 */
struct natural : pegtl::seq<pegtl::range<'1','9'>, pegtl::star<pegtl::digit>> {};
struct expr;
struct atomic_expr;
struct int_expr : pegtl::seq<integer> {};
struct float_expr : pegtl::seq<numeral> {};
struct true_expr : pegtl::seq<str_true> {};
struct false_expr : pegtl::seq<str_false> {};
struct bool_expr : pegtl::sor<true_expr, false_expr> {};
struct str_expr : pegtl::seq<literal_string> {};
struct var_expr : pegtl::seq<name> {};
struct literal_expr : pegtl::sor<bool_expr, float_expr, int_expr, str_expr, var_expr> {};
struct array_expr : pegtl::seq<type_non_void, seps, pegtl::one<'['>, seps, expr, seps, pegtl::one<']'>> {};
struct record_expr : pegtl::seq<pegtl::one<'('>, seps, pegtl::list<expr, pegtl::seq<seps, pegtl::one<','>, seps>>, pegtl::one<')'>> {};
struct paren_expr : pegtl::seq<pegtl::one<'('>, seps, expr, seps, pegtl::one<')'>> {};
struct cond_expr : pegtl::seq<str_if, seps, expr, seps, str_then, seps, expr, seps, str_else, seps, expr> {};

struct expr_tail;
struct index_tail : pegtl::seq<pegtl::one<'['>, seps, expr, seps, pegtl::one<']'>> {};
struct elem_tail : pegtl::seq<pegtl::one<'.'>, seps, natural> {};
struct expr_tail : pegtl::sor<index_tail, elem_tail> {};

struct atomic_expr_head : pegtl::sor<paren_expr, cond_expr, record_expr, array_expr, literal_expr> {};
struct atomic_expr : pegtl::seq<atomic_expr_head, pegtl::star<seps, expr_tail>> {};

struct uop_bitnot : TAO_PEGTL_STRING("~") {};
struct uop_not : TAO_PEGTL_STRING("!") {};
struct uop_minus : TAO_PEGTL_STRING("-") {};
struct uop_plus : TAO_PEGTL_STRING("+") {};
struct op_uop : pegtl::sor<uop_bitnot, uop_not, uop_minus, uop_plus> {};

struct bop_mul : TAO_PEGTL_STRING("*") {};
struct bop_div : TAO_PEGTL_STRING("/") {};
struct bop_mod : TAO_PEGTL_STRING("%") {};
struct bop_add : TAO_PEGTL_STRING("+") {};
struct bop_sub : TAO_PEGTL_STRING("-") {};
struct bop_shl : TAO_PEGTL_STRING("<<") {};
struct bop_shr : TAO_PEGTL_STRING(">>") {};
struct bop_lt : TAO_PEGTL_STRING("<") {};
struct bop_gt : TAO_PEGTL_STRING(">") {};
struct bop_le : TAO_PEGTL_STRING("<=") {};
struct bop_ge : TAO_PEGTL_STRING(">=") {};
struct bop_eq : TAO_PEGTL_STRING("==") {};
struct bop_ne : TAO_PEGTL_STRING("!=") {};
struct bop_bitand : TAO_PEGTL_STRING("&") {};
struct bop_xor : TAO_PEGTL_STRING("^") {};
struct bop_bitor : TAO_PEGTL_STRING("|") {};
struct bop_and : TAO_PEGTL_STRING("&&") {};
struct bop_or : TAO_PEGTL_STRING("||") {};
struct op_bop : pegtl::sor<bop_mul, bop_div, bop_mod, bop_add, bop_sub, bop_shl, bop_shr, bop_le, bop_ge, bop_lt, bop_gt, bop_eq, bop_ne, bop_and, bop_or, bop_bitand, bop_xor, bop_bitor> {};

struct pipe_op;
struct expr : pegtl::list<pegtl::seq<pegtl::star<op_uop, seps>, atomic_expr>, pegtl::seq<seps, pegtl::not_at<pipe_op>, op_bop, seps>> {};

/*
 * Stages and Pipelines
 */
struct statement;
struct statement_seq : pegtl::star<statement, seps> {};

struct pipeline;
struct pipe_op : TAO_PEGTL_STRING("|>") {};
struct source_op : TAO_PEGTL_STRING("|") {};
struct colon : TAO_PEGTL_STRING(":") {};

struct nop_stage : pegtl::one<'.'> {};
struct call_stage : pegtl::seq<name, seps, pegtl::one<'('>, seps, pegtl::one<')'>> {};
struct collect_stage : TAO_PEGTL_STRING("collect") {};
struct copy_stage : TAO_PEGTL_STRING("copy") {};
struct count_stage : TAO_PEGTL_STRING("count") {};
struct foreach_stage : TAO_PEGTL_STRING("foreach") {};
struct getitem_stage : pegtl::seq<pegtl::one<'.'>, seps, natural> {};
struct print_stage : TAO_PEGTL_STRING("print") {};
struct record_stage : pegtl::seq<pegtl::one<'('>, pegtl::list<pegtl::seq<seps, pegtl::sor<pipeline, name>, seps>, pegtl::one<','>>, pegtl::one<')'>> {};
struct split_stage : pegtl::seq<TAO_PEGTL_STRING("split"), seps, expr, seps, expr> {};
struct substr_stage : pegtl::seq<TAO_PEGTL_STRING("substr"), seps, expr, seps, expr> {};
struct range_stage : pegtl::seq<TAO_PEGTL_STRING("range"), pegtl::rep_min_max<1, 3, seps, expr>> {};

struct stage : pegtl::sor<call_stage, collect_stage, copy_stage, count_stage, foreach_stage, getitem_stage, print_stage, record_stage, split_stage, substr_stage, range_stage, nop_stage> {};
struct branch : pegtl::seq<pegtl::one<'{'>, seps, statement_seq, pegtl::one<'}'>> {};
struct pipeline_stage : pegtl::seq<stage, pegtl::star<seps, pipe_op, seps, pegtl::sor<branch, stage>>> {};
struct pipeline_branch : pegtl::seq<branch, pegtl::star<seps, pipe_op, seps, pegtl::sor<branch, stage>>> {};
struct pipeline : pegtl::sor<pipeline_stage, pipeline_branch> {};

struct source_args : pegtl::sor<pegtl::seq<pegtl::one<'('>, seps, pegtl::list<expr, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<')'>>, expr> {};
struct source_stmt : pegtl::seq<str_source, seps, source_args, seps, colon, seps, statement_seq, str_end> {};

/*
 * Functions
 */
struct func_decl : pegtl::seq<str_fun, seps, name, seps, pegtl::one<':'>, seps, type, seps, TAO_PEGTL_STRING("->"), seps, type> {};
struct func_stmt : pegtl::seq<func_decl, seps, statement_seq, str_end> {};

/*
 * Modules
 */
struct var_decl;
struct cell_decl;
struct pipeline_module_stmt_toplevel : pegtl::seq<source_op, seps, pipeline> {};
struct pipeline_module_stmt_nested : pegtl::seq<source_op, seps, pipeline> {};
struct pipeline_expr_stmt_toplevel : pegtl::seq<expr, seps, pipe_op, seps, pipeline> {};
struct pipeline_expr_stmt_nested : pegtl::seq<expr, seps, pipe_op, seps, pipeline> {};

struct assign_stmt : pegtl::seq<name, seps, pegtl::one<'='>, seps, expr> {};
struct assign_member_stmt : pegtl::seq<name, seps, pegtl::one<'.'>, seps, natural, seps, pegtl::one<'='>, seps, expr> {};
struct assign_expr_stmt : pegtl::seq<expr, seps, pegtl::one<'='>, seps, expr> {};

struct if_open : pegtl::seq<str_if, seps, expr, seps, colon> {};
struct elif_open : pegtl::if_must<str_elif, seps, expr, seps, colon> {};
struct else_open : pegtl::if_must<str_else, seps, colon> {};
struct if_close : pegtl::success {};
struct elif_close : pegtl::success {};
struct else_close : pegtl::success {};
struct if_stmt : pegtl::seq<if_open, seps, statement_seq, if_close, pegtl::star<seps, elif_open, seps, statement_seq, elif_close>, pegtl::opt<seps, else_open, seps, statement_seq, else_close>, seps, str_end> {};

struct statement : pegtl::sor<source_stmt, if_stmt, var_decl, cell_decl, func_stmt, assign_stmt, assign_member_stmt, assign_expr_stmt, pipeline_module_stmt_toplevel, pipeline_expr_stmt_toplevel> {};
struct module : pegtl::must<statement_seq> {};

/*
 * Assignment
 */
struct var_assign_pipeline : pegtl::seq<str_let, seps, name, seps, pegtl::one<'='>, seps, pegtl::sor<pipeline_module_stmt_nested, pipeline_expr_stmt_nested>> {};
struct var_assign_expr : pegtl::seq<str_let, seps, name, seps, pegtl::one<'='>, seps, expr> {};
struct var_decl : pegtl::sor<var_assign_pipeline, var_assign_expr> {};
struct cell_decl : pegtl::seq<str_var, seps, name, seps, pegtl::one<'='>, seps, expr> {};

/*
 * Top-level grammar
 */
struct grammar : pegtl::must<seps, module, seps, pegtl::eof> {};

#endif /* SEQ_GRAMMAR_H */
