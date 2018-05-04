#ifndef SEQ_GRAMMAR_H
#define SEQ_GRAMMAR_H

#include <tao/pegtl.hpp>

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

/*
 * General language
 */
struct short_comment : pegtl::until<pegtl::eolf> {};
struct comment : pegtl::disable<pegtl::two<'-'>, short_comment> {};

struct sep : pegtl::sor<pegtl::ascii::space, comment> {};
struct seps : pegtl::star<sep> {};

struct str_module : TAO_PEGTL_STRING("module") {};
struct str_var : TAO_PEGTL_STRING("var") {};
struct str_pipe : TAO_PEGTL_STRING("pipe") {};
struct str_end : TAO_PEGTL_STRING("end") {};
struct str_fun : TAO_PEGTL_STRING("fun") {};

struct str_keyword : pegtl::sor<str_module, str_var, str_pipe, str_end, str_fun> {};

struct name : pegtl::seq<pegtl::not_at<str_keyword>, pegtl::identifier> {};

/*
struct single : pegtl::one< 'a', 'b', 'f', 'n', 'r', 't', 'v', '\\', '"', '\'', '0', '\n' > {};
struct spaces : pegtl::seq< pegtl::one< 'z' >, pegtl::star< pegtl::space > > {};
struct hexbyte : pegtl::if_must< pegtl::one< 'x' >, pegtl::xdigit, pegtl::xdigit > {};
struct decbyte : pegtl::if_must< pegtl::digit, pegtl::rep_opt< 2, pegtl::digit > > {};
struct unichar : pegtl::if_must< pegtl::one< 'u' >, pegtl::one< '{' >, pegtl::plus< pegtl::xdigit >, pegtl::one< '}' > > {};
struct escaped : pegtl::if_must< pegtl::one< '\\' >, pegtl::sor< hexbyte, decbyte, unichar, single, spaces > > {};
struct regular : pegtl::not_one< '\r', '\n' > {};
struct character : pegtl::sor< escaped, regular > {};

template< char Q >
struct short_string : pegtl::if_must< pegtl::one< Q >, pegtl::until< pegtl::one< Q >, character > > {};
struct literal_string : pegtl::sor< short_string< '"' >, short_string< '\'' >, long_string > {};
*/

struct positive_integer : pegtl::seq<pegtl::opt<pegtl::one<'+'>>, pegtl::plus<pegtl::digit>> {};  // technically non-negative
struct negative_integer : pegtl::seq<pegtl::one<'-'>, pegtl::plus<pegtl::digit>> {};
struct integer : pegtl::sor<positive_integer, negative_integer> {};

template<typename E>
struct exponent : pegtl::opt<pegtl::if_must<E, pegtl::opt< pegtl::one<'+', '-'>>, pegtl::plus<pegtl::digit>>> {};

template<typename D, typename E>
struct numeral_three : pegtl::seq<pegtl::if_must<pegtl::one<'.'>, pegtl::plus<D>>, exponent<E>> {};
template<typename D, typename E>
struct numeral_two : pegtl::seq<pegtl::plus<D>, pegtl::opt<pegtl::one<'.'>, pegtl::star<D>>, exponent<E>> {};
template<typename D, typename E>
struct numeral_one : pegtl::sor<numeral_two<D, E>, numeral_three<D, E>> {};

struct decimal : numeral_one<pegtl::digit, pegtl::one<'e', 'E'>> {};
struct hexadecimal : pegtl::if_must<pegtl::istring<'0', 'x'>, numeral_one< pegtl::xdigit, pegtl::one<'p', 'P'>>> {};
struct numeral : pegtl::sor<hexadecimal, decimal> {};

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
struct array_type : pegtl::seq<TAO_PEGTL_STRING("Array"), seps, pegtl::one<'['>, seps, type_non_void, seps, pegtl::one<']'>> {};
struct record_type : pegtl::seq<pegtl::one<'{'>, seps, pegtl::list<type_non_void, pegtl::seq<seps, pegtl::one<','>, seps>>, seps, pegtl::one<'}'>> {};

struct type_non_void : pegtl::sor<record_type, array_type, seq_type, int_type, float_type, bool_type> {};
struct type : pegtl::sor<type_non_void, void_type> {};

/*
 * Arrays
 */
struct array_decl : pegtl::seq<type_non_void, seps, pegtl::one<'['>, seps, positive_integer, seps, pegtl::one<']'>> {};

/*
 * Stages and Pipelines
 */
struct statement;
struct statement_seq : pegtl::star<pegtl::seq<statement, seps>> {};

struct pipeline;
struct expr : pegtl::sor<pipeline, name> {};
struct assignment : pegtl::seq<str_var, seps, pegtl::one<'='>, seps, expr> {};

struct call_stage : pegtl::seq<name, seps, pegtl::one<'('>, seps, pegtl::one<')'>> {};
struct collect_stage : TAO_PEGTL_STRING("collect") {};
struct copy_stage : TAO_PEGTL_STRING("copy") {};
struct count_stage : TAO_PEGTL_STRING("count") {};
struct foreach_stage : TAO_PEGTL_STRING("foreach") {};
struct getitem_stage : pegtl::seq<pegtl::one<'@'>, seps, positive_integer> {};
struct print_stage : TAO_PEGTL_STRING("print") {};
struct record_stage : pegtl::seq<pegtl::one<'('>, pegtl::list<pegtl::seq<seps, pegtl::sor<pipeline, name>, seps>, pegtl::one<','>>, pegtl::one<')'>> {};
struct split_stage : pegtl::seq<TAO_PEGTL_STRING("split"), seps, pegtl::one<'('>, seps, integer, seps, pegtl::one<','>, seps, integer, seps, pegtl::one<')'>> {};

struct stage : pegtl::sor<call_stage, collect_stage, copy_stage, count_stage, foreach_stage, getitem_stage, print_stage, record_stage, split_stage> {};
struct branch : pegtl::seq<pegtl::one<'{'>, seps, statement_seq, pegtl::one<'}'>> {};
struct pipeline : pegtl::seq<stage, pegtl::star< pegtl::seq< seps, pegtl::one<'|'>, seps, pegtl::sor<branch, stage>>>> {};

/*
 * Functions
 */
struct func_decl : pegtl::seq<str_fun, seps, name, seps, pegtl::one<':'>, seps, type, seps, TAO_PEGTL_STRING("->"), seps, type> {};
struct func_stmt : pegtl::seq<func_decl, seps, statement_seq, str_end> {};

/*
 * Modules
 */
struct var_assign;
struct pipeline_module_stmt_toplevel : pegtl::seq<TAO_PEGTL_STRING("|>"), seps, pipeline> {};
struct pipeline_module_stmt_nested : pegtl::seq<TAO_PEGTL_STRING("|>"), seps, pipeline> {};
struct pipeline_add_stmt_toplevel : pegtl::seq<name, seps, pegtl::one<'|'>, seps, pipeline> {};
struct pipeline_add_stmt_nested : pegtl::seq<name, seps, pegtl::one<'|'>, seps, pipeline> {};
struct pipeline_array_stmt_toplevel : pegtl::seq<array_decl, seps, pegtl::one<'|'>, seps, pipeline> {};
struct pipeline_array_stmt_nested : pegtl::seq<array_decl, seps, pegtl::one<'|'>, seps, pipeline> {};
struct statement : pegtl::sor<var_assign, func_stmt, pipeline_module_stmt_toplevel, pipeline_array_stmt_toplevel, pipeline_add_stmt_toplevel> {};
struct module : pegtl::seq<str_module, seps, name, seps, statement_seq, str_end> {};

/*
 * Assignment
 */
struct var_assign_pipeline : pegtl::seq<str_var, seps, name, seps, pegtl::one<'='>, seps, pegtl::sor<pipeline_module_stmt_nested, pipeline_add_stmt_nested, pipeline_array_stmt_nested>> {};
struct var_assign_array : pegtl::seq<str_var, seps, name, seps, pegtl::one<'='>, seps, array_decl> {};
struct var_assign : pegtl::sor<var_assign_pipeline, var_assign_array> {};

/*
 * Top-level grammar
 */
struct grammar : pegtl::must<pegtl::seq<seps, module, seps>> {};

#endif /* SEQ_GRAMMAR_H */
