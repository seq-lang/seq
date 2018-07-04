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
struct seps_must : pegtl::plus<sep> {};

struct colon : TAO_PEGTL_STRING(":") {};
struct pipe_op : TAO_PEGTL_STRING("|>") {};

struct str_let : TAO_PEGTL_STRING("let") {};
struct str_var : TAO_PEGTL_STRING("var") {};
struct str_fun : TAO_PEGTL_STRING("fun") {};
struct str_if : TAO_PEGTL_STRING("if") {};
struct str_then : TAO_PEGTL_STRING("then") {};
struct str_elif : TAO_PEGTL_STRING("elif") {};
struct str_else : TAO_PEGTL_STRING("else") {};
struct str_while : TAO_PEGTL_STRING("while") {};
struct str_range : TAO_PEGTL_STRING("range") {};
struct str_source : TAO_PEGTL_STRING("source") {};
struct str_true : TAO_PEGTL_STRING("true") {};
struct str_false : TAO_PEGTL_STRING("false") {};
struct str_return : TAO_PEGTL_STRING("return") {};
struct str_break : TAO_PEGTL_STRING("break") {};
struct str_continue : TAO_PEGTL_STRING("continue") {};
struct str_as : TAO_PEGTL_STRING("as") {};
struct str_typedef : TAO_PEGTL_STRING("type") {};
struct str_class : TAO_PEGTL_STRING("class") {};
struct str_match : TAO_PEGTL_STRING("match") {};
struct str_case : TAO_PEGTL_STRING("case") {};

struct str_keyword : pegtl::sor<str_let, str_var, str_fun, str_if, str_then, str_elif, str_else, str_while, str_range, str_source, str_true, str_false, str_return, str_break, str_continue, str_as, str_typedef, str_class, str_match, str_case> {};

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
struct inf_literal : pegtl::istring<'i','n','f'> {};
struct nan_literal : pegtl::istring<'n','a','n'> {};

template< typename D >
struct number : pegtl::if_then_else<dot,
                                    pegtl::plus<D>,
                                    pegtl::seq<pegtl::plus<D>, dot, pegtl::star<D>>> {};

struct e : pegtl::one<'e','E'> {};
struct p : pegtl::one<'p','P'> {};
struct exponent : pegtl::seq<plus_minus, pegtl::plus<pegtl::digit>> {};
struct decimal : pegtl::seq<number<pegtl::digit>, pegtl::opt<e, exponent>> {};
struct hexadecimal : pegtl::seq<pegtl::one<'0'>, pegtl::one<'x','X'>, number<pegtl::xdigit>, pegtl::opt<p, exponent>> {};
struct numeral : pegtl::seq<plus_minus, pegtl::sor<hexadecimal, decimal, inf_literal, nan_literal>> {};

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
struct seq_type : TAO_PEGTL_STRING("Seq") {};
struct int_type : TAO_PEGTL_STRING("Int") {};
struct float_type : TAO_PEGTL_STRING("Float") {};
struct bool_type : TAO_PEGTL_STRING("Bool") {};
struct str_type : TAO_PEGTL_STRING("Str") {};
struct builtin_type : pegtl::sor<seq_type, int_type, float_type, bool_type, str_type> {};
struct custom_type : pegtl::seq<pegtl::not_at<pegtl::sor<str_keyword, builtin_type>>, pegtl::identifier> {};
struct realized_type : pegtl::seq<custom_type, seps, pegtl::one<'['>, seps, pegtl::list<type, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<']'>> {};

struct record_type_elem_named : pegtl::seq<name, seps, colon, seps, type> {};
struct record_type_elem_unnamed : pegtl::seq<type, pegtl::success> {};
struct record_type_elem : pegtl::sor<record_type_elem_named, record_type_elem_unnamed> {};
struct record_type : pegtl::seq<pegtl::one<'{'>, seps, pegtl::list<record_type_elem, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<'}'>> {};

struct func_type_no_void : pegtl::seq<pegtl::one<'('>, seps, pegtl::list<type, seps>, seps, TAO_PEGTL_STRING("->"), seps, type, seps, pegtl::one<')'>> {};
struct func_type_in_void : pegtl::seq<pegtl::one<'('>, seps, TAO_PEGTL_STRING("->"), seps, type, seps, pegtl::one<')'>> {};
struct func_type_out_void : pegtl::seq<pegtl::one<'('>, seps, pegtl::list<type, seps>, seps, TAO_PEGTL_STRING("->"), seps, pegtl::one<')'>> {};
struct func_type_in_out_void : pegtl::seq<pegtl::one<'('>, seps, TAO_PEGTL_STRING("->"), seps, pegtl::one<')'>> {};
struct func_type : pegtl::sor<func_type_no_void, func_type_in_void, func_type_out_void, func_type_in_out_void> {};

struct type_2 : pegtl::sor<seq_type, int_type, float_type, bool_type, str_type, record_type, func_type, realized_type, custom_type> {};
struct array_component : pegtl::seq<pegtl::one<'['>, seps, pegtl::one<']'>, seps, pegtl::opt<array_component>> {};
struct array_type : pegtl::seq<type_2, seps, array_component> {};

struct type : pegtl::sor<array_type, record_type, func_type, seq_type, int_type, float_type, bool_type, str_type, realized_type, custom_type> {};

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
struct array_expr : pegtl::seq<type, seps, pegtl::one<'['>, seps, expr, seps, pegtl::one<']'>> {};
struct construct_expr : pegtl::seq<type, seps, pegtl::one<'('>, seps, pegtl::opt<pegtl::list<expr, pegtl::seq<seps, pegtl::one<','>, seps>>>, seps, pegtl::one<')'>> {};
struct static_memb_expr : pegtl::seq<type, seps, pegtl::one<'.'>, seps, name> {};
struct default_expr : pegtl::seq<pegtl::one<'<'>, seps, type, seps, pegtl::one<'>'>> {};

struct record_expr_item_named : pegtl::seq<name, seps, colon, seps, expr> {};
struct record_expr_item_unnamed : pegtl::seq<expr, pegtl::success> {};
struct record_expr_item : pegtl::sor<record_expr_item_named, record_expr_item_unnamed> {};
struct record_expr : pegtl::seq<pegtl::one<'('>, seps, record_expr_item, seps, pegtl::one<','>, seps, pegtl::opt<pegtl::list<record_expr_item, pegtl::seq<seps, pegtl::one<','>, seps>>>, seps, pegtl::one<')'>> {};

struct paren_expr : pegtl::seq<pegtl::one<'('>, seps, expr, seps, pegtl::one<')'>> {};
struct cond_expr : pegtl::seq<str_if, seps, expr, seps, str_then, seps, expr, seps, str_else, seps, expr> {};

struct expr_tail;
struct not_a_stmt : pegtl::not_at<seps, pegtl::one<'='>, seps_must> {};  // make sure we're not actually in an assignment statement (e.g. "a[i] = c" or "a.foo = 42")
struct index_tail : pegtl::seq<pegtl::one<'['>, seps, expr, seps, pegtl::one<']'>, not_a_stmt> {};
struct call_tail : pegtl::seq<pegtl::one<'('>, seps, pegtl::opt<pegtl::list<expr, pegtl::seq<seps, pegtl::one<','>, seps>>>, seps, pegtl::one<')'>> {};
struct elem_idx_tail : pegtl::seq<pegtl::one<'.'>, seps, natural, not_a_stmt> {};
struct elem_memb_tail : pegtl::seq<pegtl::one<'.'>, seps, name, not_a_stmt> {};
struct elem_tail : pegtl::sor<elem_idx_tail, elem_memb_tail> {};
struct expr_tail : pegtl::sor<index_tail, call_tail, elem_tail> {};

struct atomic_expr_head : pegtl::sor<default_expr, array_expr, construct_expr, static_memb_expr, record_expr, paren_expr, cond_expr, literal_expr> {};
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

struct expr : pegtl::list<pegtl::seq<pegtl::star<op_uop, seps>, atomic_expr>, pegtl::seq<seps, pegtl::not_at<pipe_op>, op_bop, seps>> {};

/*
 * Patterns
 */
struct pattern;
struct int_pattern : pegtl::seq<integer> {};
struct float_pattern : pegtl::seq<numeral> {};
struct true_pattern : pegtl::seq<str_true> {};
struct false_pattern : pegtl::seq<str_false> {};
struct record_pattern : pegtl::seq<pegtl::one<'('>, seps, pegtl::sor<pegtl::seq<pattern, pegtl::plus<seps, pegtl::one<','>, seps, pattern>>, pegtl::seq<pattern, seps, pegtl::one<','>>>, seps, pegtl::one<')'>> {};
struct wildcard_pattern : pegtl::seq<name> {};
struct range_pattern : pegtl::seq<integer, seps, TAO_PEGTL_STRING("..."), seps, integer> {};
struct paren_pattern : pegtl::seq<pegtl::one<'('>, seps, pattern, seps, pegtl::one<')'>> {};
struct pattern0 : pegtl::sor<range_pattern, int_pattern, float_pattern, true_pattern, false_pattern, record_pattern, wildcard_pattern, paren_pattern> {};
struct pattern : pegtl::list<pattern0, pegtl::seq<seps, pegtl::one<'|'>, seps>> {};

/*
 * Stages and Pipelines
 */
struct statement;
struct statement_seq : pegtl::star<statement, pegtl::opt<seps, pegtl::one<';'>>, seps> {};
struct block_single : pegtl::seq<pegtl::one<':'>, seps, statement, pegtl::opt<seps, pegtl::one<';'>>> {};
struct block_multi : pegtl::seq<pegtl::one<'{'>, seps, statement_seq, seps, pegtl::one<'}'>> {};
struct block : pegtl::sor<block_single, block_multi> {};

struct pipeline;

struct nop_stage : pegtl::one<'.'> {};
struct len_stage : TAO_PEGTL_STRING("len") {};
struct revcomp_stage : TAO_PEGTL_STRING("revcomp") {};
struct call_stage : pegtl::seq<name> {};
struct collect_stage : TAO_PEGTL_STRING("collect") {};
struct copy_stage : TAO_PEGTL_STRING("copy") {};
struct count_stage : TAO_PEGTL_STRING("count") {};
struct foreach_stage : TAO_PEGTL_STRING("foreach") {};
struct getitem_stage : pegtl::seq<pegtl::one<'.'>, seps, natural> {};
struct print_stage : TAO_PEGTL_STRING("print") {};
struct split_stage : pegtl::seq<TAO_PEGTL_STRING("split"), seps, expr, seps, expr> {};
struct substr_stage : pegtl::seq<TAO_PEGTL_STRING("substr"), seps, expr, seps, expr> {};
struct filter_stage : pegtl::seq<TAO_PEGTL_STRING("filter"), seps, expr> {};
struct chunk_stage : pegtl::seq<TAO_PEGTL_STRING("chunk"), seps, pegtl::opt<expr>> {};

struct record_stage_elem_pipeline : pegtl::seq<pipe_op, seps, pipeline> {};
struct record_stage_elem_expr_pipeline : pegtl::seq<expr, seps, pipe_op, seps, pipeline> {};
struct record_stage_elem_expr : pegtl::seq<expr> {};
struct record_stage : pegtl::seq<pegtl::one<'('>, seps, pegtl::list<pegtl::sor<record_stage_elem_pipeline, record_stage_elem_expr_pipeline, record_stage_elem_expr>, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<')'>> {};

struct stage_raw : pegtl::sor<len_stage, revcomp_stage, collect_stage, copy_stage, count_stage, foreach_stage, getitem_stage, print_stage, split_stage, substr_stage, filter_stage, chunk_stage, record_stage, nop_stage, call_stage> {};
struct stage_as : pegtl::seq<str_as, seps, name> {};
struct stage : pegtl::seq<stage_raw, pegtl::opt<seps, stage_as>> {};
struct branch : pegtl::seq<block> {};

struct pipeline_component;
struct pipeline_component_branch;
struct pipeline_component_stage : pegtl::seq<stage, pegtl::opt<seps, pegtl::sor<pipeline_component_branch, pegtl::seq<pipe_op, seps, pipeline_component_stage>>>> {};
struct pipeline_component_branch : pegtl::seq<branch, pegtl::opt<seps, pipe_op, pipeline_component>> {};
struct pipeline_component : pegtl::sor<pipeline_component_stage, pipeline_component_branch> {};

struct pipeline_stage : pegtl::seq<pipeline_component_stage> {};
struct pipeline_branch : pegtl::seq<pipeline_component_branch> {};
struct pipeline : pegtl::sor<pipeline_stage, pipeline_branch> {};

struct while_args : pegtl::if_must<str_while, seps, expr> {};
struct while_body : pegtl::seq<block> {};
struct while_stmt : pegtl::if_must<while_args, seps, while_body> {};

struct range_args : pegtl::if_must<str_range, seps, pegtl::rep_min_max<1, 3, seps, expr>> {};
struct range_as : pegtl::opt<str_as, seps, name> {};
struct range_body : pegtl::seq<block> {};
struct range_stmt : pegtl::if_must<range_args, seps, range_as, seps, range_body> {};

struct source_args : pegtl::if_must<str_source, seps, pegtl::list<expr, seps>> {};
struct source_as : pegtl::opt<str_as, seps, name> {};
struct source_body : pegtl::seq<block> {};
struct source_stmt : pegtl::if_must<source_args, seps, source_as, seps, source_body> {};

struct typedef_stmt : pegtl::if_must<str_typedef, seps, pegtl::not_at<builtin_type>, name, seps, pegtl::one<'='>, seps, type> {};

/*
 * Functions
 */
struct func_args : pegtl::opt<pegtl::seq<pegtl::one<'('>, seps, pegtl::opt<pegtl::list<pegtl::seq<name, seps, pegtl::one<':'>, seps, type>, pegtl::seq<seps, pegtl::one<','>, seps>>>, seps, pegtl::one<')'>>> {};
struct func_decl : pegtl::seq<str_fun, seps, name, seps, func_args, seps, TAO_PEGTL_STRING("->"), seps, type> {};
struct func_decl_out_void : pegtl::seq<str_fun, seps, name, seps, func_args> {};
struct func_decl_in_out_void : pegtl::seq<str_fun, seps, name> {};
struct func_stmt : pegtl::seq<pegtl::sor<func_decl, func_decl_out_void, func_decl_in_out_void>, seps, block> {};

/*
 * Classes
 */
struct class_generics : pegtl::seq<pegtl::one<'['>, seps, pegtl::list<name, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<']'>> {};
struct class_open : pegtl::if_must<str_class, seps, name, pegtl::opt<seps, class_generics>> {};
struct class_type : pegtl::seq<pegtl::one<'('>, seps, pegtl::list<record_type_elem_named, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<')'>> {};
struct class_stmt : pegtl::if_must<class_open, seps, pegtl::opt<class_type>, seps, pegtl::one<'{'>, seps, pegtl::opt<pegtl::list<func_stmt, seps>, seps>, pegtl::one<'}'>> {};

/*
 * Modules
 */
struct var_decl;
struct cell_decl;
struct pipeline_expr_stmt_toplevel : pegtl::seq<expr, seps, pipe_op, seps, pipeline> {};
struct pipeline_expr_stmt_nested : pegtl::seq<expr, seps, pipe_op, seps, pipeline> {};

struct assign_stmt : pegtl::seq<name, seps, pegtl::one<'='>, seps, expr> {};
struct assign_member_idx_stmt : pegtl::seq<expr, seps, pegtl::one<'.'>, seps, natural, seps, pegtl::one<'='>, seps, expr> {};
struct assign_member_stmt : pegtl::seq<expr, seps, pegtl::one<'.'>, seps, name, seps, pegtl::one<'='>, seps, expr> {};
struct assign_array_stmt : pegtl::seq<expr, seps, pegtl::one<'['>, seps, expr, seps, pegtl::one<']'>, seps, pegtl::one<'='>, seps, expr> {};

struct if_open : pegtl::seq<str_if, seps, expr> {};
struct elif_open : pegtl::if_must<str_elif, seps, expr> {};
struct else_open : pegtl::seq<str_else> {};
struct if_close : pegtl::success {};
struct elif_close : pegtl::success {};
struct else_close : pegtl::success {};
struct if_stmt : pegtl::seq<if_open, seps, block, if_close, pegtl::star<seps, elif_open, seps, block, elif_close>, pegtl::opt<seps, else_open, seps, block, else_close>> {};

struct match_open : pegtl::seq<str_match, seps, expr> {};
struct case_open : pegtl::seq<str_case, seps, pattern> {};
struct case_close : pegtl::success {};
struct match_stmt : pegtl::seq<match_open, seps, pegtl::one<'{'>, seps, pegtl::list<pegtl::seq<case_open, seps, block, case_close>, seps>, seps, pegtl::one<'}'>> {};

struct return_stmt : pegtl::seq<str_return, pegtl::opt<seps, expr>> {};
struct break_stmt : pegtl::seq<str_break> {};
struct continue_stmt : pegtl::seq<str_continue> {};

struct expr_stmt : pegtl::seq<expr> {};

struct statement : pegtl::sor<class_stmt, typedef_stmt, range_stmt, source_stmt, while_stmt, return_stmt, break_stmt, continue_stmt, var_decl, cell_decl, func_stmt, assign_stmt, assign_member_idx_stmt, assign_member_stmt, assign_array_stmt, pipeline_expr_stmt_toplevel, expr_stmt, if_stmt, match_stmt> {};
struct module : pegtl::must<statement_seq> {};

/*
 * Assignment
 */
struct var_assign_pipeline : pegtl::seq<str_let, seps, name, seps, pegtl::one<'='>, seps, pipeline_expr_stmt_nested> {};
struct var_assign_expr : pegtl::seq<str_let, seps, name, seps, pegtl::one<'='>, seps, expr> {};
struct var_decl : pegtl::sor<var_assign_pipeline, var_assign_expr> {};
struct cell_decl : pegtl::seq<str_var, seps, name, seps, pegtl::one<'='>, seps, expr> {};

/*
 * Top-level grammar
 */
struct grammar : pegtl::must<seps, module, seps, pegtl::eof> {};

#endif /* SEQ_GRAMMAR_H */
