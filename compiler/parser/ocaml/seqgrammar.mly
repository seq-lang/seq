(* *****************************************************************************
 * Seq.Parser: Menhir grammar description of Seq language
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

%{
  open Ast
%}

/* constants */
%token <string * string> INT_S SEQ
%token <float * string> FLOAT_S
%token <string> STRING ID FSTRING KMER EXTERN PYDEF_RAW
/* blocks & parentheses */
%token INDENT DEDENT EOF NL DOT COLON SEMICOLON COMMA OF
%token LP RP /* () */ LS RS /* [] */ LB RB /* {} */
/* keywords */
%token FOR WHILE CONTINUE BREAK IF ELSE ELIF MATCH CASE EXTEND
%token DEF RETURN YIELD LAMBDA PYDEF TYPE CLASS TYPEOF AS PTR
%token IMPORT FROM GLOBAL PRINT PASS ASSERT DEL TRUE FALSE NONE
%token TRY EXCEPT FINALLY THROW WITH
/* operators */
%token<string> EQ ELLIPSIS ADD SUB MUL DIV FDIV POW MOD
%token<string> PLUSEQ MINEQ MULEQ DIVEQ FDIVEQ POWEQ MODEQ AT GEQ
%token<string> AND OR NOT IS ISNOT IN NOTIN EEQ NEQ LESS LEQ GREAT
%token<string> PIPE PPIPE SPIPE B_AND B_OR B_XOR B_NOT B_LSH B_RSH
%token<string> LSHEQ RSHEQ ANDEQ OREQ XOREQ
/* operator precedence */
%left B_OR
%left B_XOR
%left B_AND
%left B_LSH B_RSH
%left ADD SUB
%left MUL DIV FDIV MOD
%left POW AT
/* main entry */
%start <Ast.tstmt Ast.ann list> program
%%
program: statement+ EOF { List.concat $1 }

/* 0. Utilities (http://gallium.inria.fr/blog/lr-lists/) */
reverse_separated_nonempty_llist(separator, X):
  X { [$1] } | reverse_separated_nonempty_llist(separator, X) separator X { $3 :: $1 }
%inline reverse_separated_llist(separator, X):
  { [] } | reverse_separated_nonempty_llist(separator, X) { $1 }
%inline separated_llist(separator, X): reverse_separated_llist(separator, X) { List.rev $1 }
%inline FL(delim, X): separated_llist(delim, X) delim? { $1 } /* list that has optional delim at the end */
%inline FLNE(delim, X): /* non-empty list that has optional delim at the end */
  X { [$1] } | X delim separated_llist(delim, X) delim? { $1 :: $3 }
%inline FL_HAS(delim, X): /* non-empty list that signals if there is an optional delim at the end */
  | separated_llist(delim, X) delim? { $1, match $2 with Some _ -> true | None -> false }

/* 1. Atoms  */
atom:
  | NONE { $loc, Empty () }
  | ID { $loc, Id $1 }
  | INT_S { $loc, Int $1 }
  | FLOAT_S { $loc, Float $1 }
  | STRING+ { $loc, String (String.concat "" $1) }
  | FSTRING+ { $loc, FString (String.concat "" $1) }
  | SEQ+
    { $loc, Seq (fst (List.hd $1), String.concat "" @@ List.map (fun (i, j) ->
        if i <> fst (List.hd $1)
          then raise (Ast.SyntaxError ("cannot concatenate different types", $startpos));
        j) $1) }
  | KMER { $loc, Kmer $1 }
  | bool { $loc, Bool $1 }
  | tuple { $1 }
  | LS FL(COMMA, expr) RS { $loc, List $2 }
  | LB FLNE(COMMA, expr) RB { $loc, Set $2 }
  | LB FL(COMMA, dictitem) RB { $loc, Dict $2 }
  | LP expr RP { $2 }
  | LP expr comprehension RP { $loc, Generator ($2, $3) }
  | LS expr comprehension RS { $loc, ListGenerator ($2, $3) }
  | LB expr comprehension RB { $loc, SetGenerator ($2, $3) }
  | LB dictitem comprehension RB { $loc, DictGenerator ($2, $3) }
  | MUL ID { $loc, Unpack ($loc($2), Id $2) }
  | MUL tuple { $loc, Unpack $2 }
bool: TRUE { true } | FALSE { false }
tuple:
  | LP RP { $loc, Tuple [] }
  | LP expr COMMA RP { $loc, Tuple [$2] }
  | LP expr COMMA FLNE(COMMA, expr) RP { $loc, Tuple ($2 :: $4) }
dictitem: expr COLON expr { $1, $3 }
comprehension: FOR FLNE(COMMA, ID) IN pipe_expr comprehension_if* comprehension?
  { $loc, { var = $2; gen = flat_pipe $4; cond = $5; next = $6 } }
comprehension_if: IF pipe_expr { $loc, snd (flat_pipe $2) }

/* 2. Expressions */
expr:
  | pipe_expr { flat_pipe $1 }
  | pipe_expr IF pipe_expr ELSE expr { $loc, IfExpr (flat_pipe $3, flat_pipe $1, $5) }
  | TYPEOF LP expr RP { $loc, TypeOf $3 }
  | PTR LP expr RP { $loc, Ptr $3 }
  | LAMBDA separated_list(COMMA, ID) COLON expr { $loc, Lambda ($2, $4) }
expr_list: separated_nonempty_list(COMMA, expr) { $1 }

/* The following rules are defined in the order of operator precedence:
     pipes -> booleans -> conditionals -> arithmetics */
pipe_expr:
  | o = bool_expr { fst o, ["", o] }
  | l = bool_expr; p = PIPE; r = pipe_expr
  | l = bool_expr; p = PPIPE; r = pipe_expr
  | l = bool_expr; p = SPIPE; r = pipe_expr { $loc, (p, l) :: (snd r) }
bool_expr:
  | bool_and_expr { $1 }
  | bool_and_expr OR bool_expr { $loc, Binary ($1, $2, $3) }
bool_and_expr:
  | cond_expr { flat_cond $1 }
  | cond_expr AND bool_and_expr { $loc, Binary (flat_cond $1, $2, $3) }
cond_expr:
  | arith_expr { $loc, Cond (snd $1) }
  | NOT cond_expr { $loc, Cond (Unary ("!", flat_cond $2)) }
  | arith_expr cond_op cond_expr { $loc, CondBinary ($1, $2, $3) }
%inline cond_op:
  LESS | LEQ | GREAT | GEQ | EEQ | NEQ | IS | ISNOT | IN | NOTIN { $1 }

arith_expr:
  | arith_term { $1 }
  | arith_expr arith_op arith_expr { $loc, Binary ($1, $2, $3) }
  | SUB+ arith_term | B_NOT+ arith_term | ADD+ arith_term /* Unary */
    { let cnt = List.length $1 in
      $loc, match cnt mod 2, List.hd $1, snd $2 with
      | 1, "~", _ -> Unary ("~", $2)
      | 1, "-", Int (f, s) -> Int ("-" ^ f, s)
      | 1, "-", Float (f, s) -> Float (-.f, s)
      | 1, "-", _ -> Unary ("-", $2)
      | _ -> snd $2 }
%inline arith_op:
  ADD | SUB | MUL | DIV | FDIV | MOD | POW | AT | B_AND | B_OR | B_XOR | B_LSH | B_RSH { $1 }
arith_term:
  | atom { $1 }
  | arith_term LP FL(COMMA, call_term) RP { $loc, Call ($1, $3) }
  | arith_term LP expr comprehension RP /* Generator: foo(x for x in y) */
    { $loc, Call ($1, [None, (($startpos($2), $endpos($5)), Generator ($3, $4))]) }
  | arith_term LS index_term RS { $loc, Index ($1, $3) }
  | arith_term LS index_term COMMA RS { $loc, Index ($1, ($loc($3), Tuple [$3])) }
  | arith_term LS index_term COMMA FLNE(COMMA, index_term) RS
    { $loc, Index ($1, (($startpos($3), $endpos($5)), Tuple ($3 :: $5))) }
  | arith_term DOT ID { $loc, Dot ($1, $3) }
  | LP YIELD RP { $loc, YieldTo () }
call_term:
  | ELLIPSIS { None, ($loc, Ellipsis ()) }
  | expr { None, $1 }
  | ID EQ expr { Some $1, $3 }
  | ID EQ ELLIPSIS { Some $1, ($loc($3), Ellipsis ()) }
index_term:
  | expr { $1 }
  | expr? COLON expr? { $loc, Slice ($1, $3, None) }
  | expr? COLON expr? COLON expr? { $loc, Slice ($1, $3, $5) }

/* 3. Statements */
statement:
  | FLNE(SEMICOLON, small_statement) NL { List.concat $1 }
  | single_statement { [$1] }
  | func_statement { $1 }
small_statement:
  | expr_list { List.map (fun expr -> fst expr, Expr expr) $1 }
  | small_single_statement { [$1] }
  | DEL FLNE(COMMA, expr) { List.map (fun e -> fst e, Del e) $2 }
  | ASSERT FLNE(COMMA, expr) { List.map (fun e -> fst e, Assert e) $2 }
  | GLOBAL FLNE(COMMA, ID) { List.map (fun e -> $loc, Global e) $2 }
  /* | PREFETCH FLNE(COMMA, expr) { List.map (fun e -> fst e, Prefetch e) $2 } */
  | print_statement { $1 }
  | import_statement { $1 }
  | assign_statement { $1 }
small_single_statement:
  | PASS { $loc, Pass () }
  | BREAK { $loc, Break () }
  | CONTINUE { $loc, Continue () }
  | RETURN separated_list(COMMA, expr)
    { $loc, Return (match $2 with [] -> None | [e] -> Some e | l -> Some ($loc, Tuple l)) }
  | YIELD separated_list(COMMA, expr)
    { $loc, Yield (match $2 with [] -> None | [e] -> Some e | l -> Some ($loc, Tuple l)) }
  | YIELD FROM expr { $loc, YieldFrom $3 }
  /* | TYPE ID EQ expr { $loc, TypeAlias ($2, $4) } */
  | THROW expr { $loc, Throw $2 }
print_statement:
  /* | PRINT { [$loc, Print ($loc, String "\n")] } */
  | PRINT FL_HAS(COMMA, expr)
    { let term, len = (if snd $2 then " " else "\n"), List.length (fst $2) in
      if len = 0 then [$loc, Print ($loc, String term)]
      else List.concat (List.mapi (fun i e -> [fst e, Print e;
                                               fst e, Print (fst e, String (if i < len - 1 then " " else term))])
                                  (fst $2)) }

single_statement:
  | NL { $loc, Pass () }
  | WHILE expr COLON suite { $loc, While ($2, $4) }
  | FOR FLNE(COMMA, ID) IN expr COLON suite {
    let var = $loc($2), match $2 with [e] -> Id e | l -> Tuple (List.map (fun i -> $loc($2), Id i) l) in
    $loc, For (var, $4, $6) }
  /* TODO: allow any lhs-expression without parentheses */
  | FOR tuple IN expr COLON suite { $loc, For ($2, $4, $6) }
  | IF expr COLON suite { $loc, If [Some $2, $4] }
  | IF expr COLON suite elif_suite { $loc, If ((Some $2, $4) :: $5) }
  | MATCH expr COLON NL INDENT case_suite DEDENT { $loc, Match ($2, $6) }
  /* | decl_statement  */
  | try_statement | with_statement | class_statement { $1 }
suite:
  | FLNE(SEMICOLON, small_statement) NL { List.concat $1 }
  | NL INDENT statement+ DEDENT { List.concat $3 }
elif_suite:
  | ELIF expr COLON suite { [Some $2, $4] }
  | ELSE COLON suite { [None, $3] }
  | ELIF expr COLON suite elif_suite { (Some $2, $4) :: $5 }
case_suite: case { [$1] } | case case_suite { $1 :: $2 }
case:
  | CASE case_or COLON suite { $2, $4 }
  | CASE case_or IF bool_expr COLON suite { ($loc, GuardedPattern ($2, $4)), $6 }
  | CASE case_or AS ID COLON suite { ($loc, BoundPattern ($4, $2)), $6 }
case_or: separated_nonempty_list(OR, case_type) { match $1 with [p] -> p | l -> $loc, OrPattern l }
case_type:
  | ELLIPSIS { $loc, StarPattern () }
  | ID { $loc, WildcardPattern (match $1 with "_" -> None | s -> Some s) }
  | case_int { $loc, IntPattern $1 }
  | bool { $loc, BoolPattern $1 }
  | STRING { $loc, StrPattern $1 }
  | SEQ { $loc, SeqPattern (snd $1) } /* TODO: no protein matching? */
  | LP separated_nonempty_list(COMMA, case_or) RP { $loc, TuplePattern $2 }
  | LS FL(COMMA, case_or) RS { $loc, ListPattern $2 }
  | case_int ELLIPSIS case_int { $loc, RangePattern($1, $3) }
case_int:
  | INT_S { Int64.of_string (fst $1) }
  | ADD INT_S { Int64.of_string (fst $2) }
  | SUB INT_S { Int64.neg (Int64.of_string (fst $2)) }

import_statement:
  | FROM dot_term IMPORT MUL
    { [$loc, Import ((flatten_dot ~sep:"/" $2, None), ["*", None])] }
  | FROM dot_term IMPORT FLNE(COMMA, import_term)
    { let what = List.map (fun (_, (what, ias)) -> (flatten_dot ~sep:"/" what, ias)) $4 in
      [$loc, Import ((flatten_dot ~sep:"/" $2, None), what)] }
  | IMPORT FLNE(COMMA, import_term)
    { List.map (fun (pos, (from, import_as)) -> pos, Import ((flatten_dot ~sep:"/" from, import_as), [])) $2 }
import_term:
  | dot_term { $loc, ($1, None) }
  | dot_term AS ID { $loc, ($1, Some $3) }
dot_term: ID { $loc, Id $1 } | dot_term DOT ID { $loc, Dot ($1, $3) }

assign_statement:
  | expr aug_eq expr { [$loc, AssignEq ($1, $3, String.sub $2 0 (String.length $2 - 1))] }
  | ID COLON expr EQ expr { [$loc, Assign (($loc($1), Id $1), $5, Some $3)] }
  | expr_list EQ separated_nonempty_list(EQ, expr_list)
    { let all = List.map (function [l] -> l | l -> $loc, Tuple l) (List.rev ($1 :: $3)) in
      List.rev @@ List.map (fun i -> $loc, Assign (i, List.hd all, None)) (List.tl all) }
%inline aug_eq: PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ | LSHEQ | RSHEQ | ANDEQ | OREQ | XOREQ { $1 }
decl_statement: ID COLON expr NL { $loc, Declare ($loc, { name = $1; typ = Some $3; default = None }) }

try_statement: TRY COLON suite catch* finally? { $loc, Try ($3, $4, opt_val $5 []) }
catch:
  /* TODO: except (RuntimeError, TypeError, NameError) */
  | EXCEPT COLON suite { $loc, { exc = None; var = None; stmts = $3 } }
  | EXCEPT expr COLON suite { $loc, { exc = Some $2; var = None; stmts = $4 } }
  | EXCEPT expr AS ID COLON suite { $loc, { exc = Some $2; var = Some $4; stmts = $6 } }
finally: FINALLY COLON suite { $3 }

with_statement: WITH FLNE(COMMA, with_clause) COLON suite { $loc, With ($2, $4) }
with_clause: expr { $1, None } | expr AS ID { $1, Some $3 }

func_statement:
  | decorator* func
    { List.map (function (pos, Function f) -> (pos, Function { f with fn_attrs = $1 }) | f -> f) $2 }
  | pyfunc { $1 }
func:
  | func_def COLON suite
    { [$loc, Function { $1 with fn_stmts = $3 }] }
  | extern_from? EXTERN FLNE(COMMA, extern_what) NL
    { List.map (fun (pos, e) -> pos, ImportExtern { e with lang = $2; e_from = $1 }) $3 }
func_def:
  | DEF ID generic_list? LP FL(COMMA, typed_param) RP func_ret_type?
    { { fn_name = $2; fn_rettyp = $7; fn_generics = opt_val $3 []; fn_args = $5; fn_stmts = []; fn_attrs = [] } }
typed_param: ID param_type? default_val? { $loc, { name = $1; typ = $2; default = $3 } }
generic_list: LS FLNE(COMMA, typed_param) RS { $2 }
default_val: EQ expr { $2 }
param_type: COLON expr { $2 }
func_ret_type: OF expr { $2 }
extern_from: FROM dot_term { $2 }
extern_what:
  | ID LP FL(COMMA, extern_param) RP func_ret_type? extern_as?
    { let e_typ = match $5 with Some typ -> typ | None -> $loc($4), Id "void" in
      $loc, { lang = ""; e_from = None; e_name = $1; e_typ; e_args = $3; e_as = $6 } }
extern_param:
  | expr { $loc, { name = ""; typ = Some $1; default = None } }
  | ID param_type { $loc, { name = $1; typ = Some $2; default = None } }
extern_as: AS ID { $2 }
decorator: AT ID NL { $loc, $2 } /* AT dot_term NL | AT dot_term LP FL(COMMA, expr) RP NL */
pyfunc: PYDEF ID LP FL(COMMA, typed_param) RP func_ret_type? COLON PYDEF_RAW { [$loc, PyDef ($2, $6, $4, $8)] }

class_statement: cls | extend | typ { $1 }
cls:
  | decorator* CLASS ID generic_list? COLON NL INDENT dataclass_member+ DEDENT
    { let args = List.rev @@ List.fold_left
        (fun acc i -> match i with Some (_, Declare d) -> d :: acc | _ -> acc) [] $8
      in
      let members = List.rev @@ List.fold_left
        (fun acc i -> match i with Some (_, Declare _) | None -> acc | Some p -> p :: acc) [] $8
      in
      $loc, Class { class_name = $3; generics = opt_val $4 []; args; members; attrs = $1 } }
dataclass_member: class_member { $1 } | decl_statement { Some $1 }
class_member:
  | PASS NL | STRING NL { None }
  | func_statement { Some (List.hd $1) }
  | decorator* func_def { Some ($loc, Function { $2 with fn_attrs = $1 }) }
extend: EXTEND expr COLON NL INDENT class_member+ DEDENT { $loc, Extend ($2, filter_opt $6) }
typ:
  | type_head NL { $loc, Type (snd $1) }
  | type_head COLON NL INDENT class_member+ DEDENT { $loc, Type { (snd $1) with members = filter_opt $5 } }
type_head:
  | decorator* TYPE ID generic_list? LP FL(COMMA, typed_param) RP
    { $loc, { class_name = $3; generics = opt_val $4 []; args = $6; members = []; attrs = $1 } }
