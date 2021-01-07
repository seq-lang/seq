(* *****************************************************************************
 * Seq.Parser: Menhir grammar description of Seq language
 *
 * Author: inumanag
 * License: see LICENSE
 *
 * TODO: Add comment AST nodes
 * *****************************************************************************)

%{ open Ast %}

/* constants */
%token <string * string> INT STRING
%token <float * string> FLOAT
%token <string> ID EXTERN
/* blocks & parentheses */
%token INDENT DEDENT EOF NL DOT COLON SEMICOLON COMMA OF
%token LP RP /* () */ LS RS /* [] */ LB RB /* {} */
/* keywords */
%token IF ELSE ELIF MATCH CASE FOR WHILE CONTINUE BREAK TRY EXCEPT FINALLY THROW WITH
%token DEF RETURN YIELD LAMBDA CLASS TYPEOF AS
%token IMPORT FROM GLOBAL PRINT PRINTLP PASS ASSERT DEL TRUE FALSE NONE
/* %token ARROW */
/* operators */
%token<string> EQ WALRUS ELLIPSIS ADD SUB MUL DIV FDIV POW MOD
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
  | INT { $loc, Int $1 }
  | FLOAT { $loc, Float $1 }
  | string { $loc, String $1 }
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
  | MUL ID { $loc, Star ($loc($2), Id $2) }
  | MUL tuple { $loc, Star $2 }
bool: TRUE { true } | FALSE { false }
string: STRING+ {
  fst (List.hd $1),
  List.map (fun (i, j) ->
      if i <> fst (List.hd $1)
      then raise (Ast.SyntaxError ("cannot concatenate different types", $startpos));
      j) $1 |> String.concat ""
  }
tuple:
  | LP RP { $loc, Tuple [] }
  | LP expr COMMA RP { $loc, Tuple [$2] }
  | LP expr COMMA FLNE(COMMA, expr) RP { $loc, Tuple ($2 :: $4) }
dictitem: expr COLON expr { $1, $3 }
comprehension: FOR lassign IN pipe_expr comprehension_if* comprehension?
  { $loc, { var = $2; gen = flat_pipe $4; cond = $5; next = $6 } }
comprehension_if: IF pipe_expr { $loc, snd (flat_pipe $2) }

/* 2. Expressions */
expr:
  | pipe_expr { flat_pipe $1 }
  | pipe_expr IF pipe_expr ELSE expr { $loc, IfExpr (flat_pipe $3, flat_pipe $1, $5) }
  | LAMBDA separated_list(COMMA, ID) COLON expr { $loc, Lambda ($2, $4) }
  | walrus { $1 }
  /* | LP separated_list(COMMA, ID) ARROW pipe_expr RP { $loc, Lambda ($2, $4) } */
expr_list: separated_nonempty_list(COMMA, expr) { $1 }

lassign_term:
  | ID { $loc, Id $1 }
  | LS FL(COMMA, lassign_term) RS { $loc, List $2 }
  | LP FL(COMMA, lassign_term) RP { $loc, Tuple $2 }
  | MUL ID { $loc, Star ($loc($2), Id $2) }
lassign: FLNE(COMMA, lassign_term) { match $1 with [l] -> l | l -> $loc, Tuple l }

/* The following rules are defined in the order of operator precedence:
     pipes -> booleans -> conditionals -> arithmetics */
pipe_expr:
  | o = bool_expr { fst o, ["", o] }
  | l = bool_expr; p = PIPE; r = pipe_expr
  | l = bool_expr; p = PPIPE; r = pipe_expr
  | l = bool_expr; p = SPIPE; r = pipe_expr { $loc, (p, l) :: (snd r) }
bool_expr:
  | bool_and_expr { $1 }
  | bool_and_expr OR bool_expr { $loc, Binary ($1, $2, $3, false) }
bool_and_expr:
  | cond_expr { flat_cond $1 }
  | cond_expr AND bool_and_expr { $loc, Binary (flat_cond $1, $2, $3, false) }
cond_expr:
  | arith_expr { $loc, Cond (snd $1) }
  | NOT cond_expr { $loc, Cond (Unary ("!", flat_cond $2)) }
  | arith_expr cond_op cond_expr { $loc, CondBinary ($1, $2, $3) }
%inline cond_op:
  LESS | LEQ | GREAT | GEQ | EEQ | NEQ | IS | ISNOT | IN | NOTIN { $1 }

arith_expr:
  | arith_term { $1 }
  | arith_expr arith_op arith_expr { $loc, Binary ($1, $2, $3, false) }
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
  | PRINTLP FL(COMMA, call_term) RP { $loc, Call (($loc($1), Id "echo"), $2) }
  | arith_term LP FL(COMMA, call_term) RP { $loc, Call ($1, $3) }
  | arith_term LP expr comprehension RP /* Generator: foo(x for x in y) */
    { $loc, Call ($1, [None, (($startpos($2), $endpos($5)), Generator ($3, $4))]) }
  | arith_term LS index_term RS { $loc, Index ($1, $3) }
  | arith_term LS index_term COMMA RS { $loc, Index ($1, ($loc($3), Tuple [$3])) }
  | arith_term LS index_term COMMA FLNE(COMMA, index_term) RS
    { $loc, Index ($1, (($startpos($3), $endpos($5)), Tuple ($3 :: $5))) }
  | arith_term DOT ID { $loc, Dot ($1, $3) }
  | TYPEOF LP expr RP { $loc, TypeOf $3 }
  | LP YIELD RP { $loc, YieldTo () }
  | ELLIPSIS { $loc, Ellipsis () }
  | INT ELLIPSIS INT { $loc, Range (($loc($1), Int $1), ($loc($3), Int $3)) }
call_term:
  /* | ELLIPSIS { None, ($loc, Ellipsis ()) } */
  | expr { None, $1 }
  | ID EQ expr { Some $1, $3 }
  /* | ID EQ ELLIPSIS { Some $1, ($loc($3), Ellipsis ()) } */
index_term:
  | expr { $1 }
  | expr? COLON expr? { $loc, Slice ($1, $3, None) }
  | expr? COLON expr? COLON expr? { $loc, Slice ($1, $3, $5) }
walrus:
  | ID WALRUS expr { $loc, AssignExpr (($loc($1), Id $1), $3) }
  /* | LP FL(COMMA, ID) RP WALRUS expr { $loc, AssignExpr ($loc($1), Id $1) $3 } */

/* 3. Statements */
statement:
  | FLNE(SEMICOLON, small_statement) NL { List.concat $1 }
  | single_statement { [$1] }
  | func_statement { $1 }
small_statement:
  | expr_list { List.map (fun expr -> fst expr, Expr expr) $1 }
  | small_single_statement { [$1] }
  | DEL FLNE(COMMA, expr) { List.map (fun e -> fst e, Del e) $2 }
  | ASSERT expr { [$loc, Assert ($2, None)] }
  | ASSERT expr COMMA STRING { [$loc, Assert ($2, Some ($loc($4), String $4))] }
  | GLOBAL FLNE(COMMA, ID) { List.map (fun e -> $loc, Global e) $2 }
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
  | THROW expr { $loc, Throw $2 }
print_statement:
  | PRINT FL_HAS(COMMA, expr)
    { let term, len = (if snd $2 then " " else "\n"), List.length (fst $2) in
      if len = 0 then [$loc, Print ($loc, String ("", term))]
      else List.concat (List.mapi (fun i e -> [fst e, Print e;
                                               fst e, Print (fst e, String ("", if i < len - 1 then " " else term))])
                                  (fst $2)) }

single_statement:
  | NL { $loc, Pass () }
  | WHILE expr COLON suite { $loc, While ($2, $4, []) }
  | WHILE expr COLON suite ELSE COLON suite { $loc, While ($2, $4, $7) }
  | WHILE expr COLON suite ELSE NOT BREAK COLON suite { $loc, While ($2, $4, $9) }
  | FOR lassign IN expr COLON suite { $loc, For ($2, $4, $6, []) }
  | FOR lassign IN expr COLON suite ELSE COLON suite { $loc, For ($2, $4, $6, $9) }
  | FOR lassign IN expr COLON suite ELSE NOT BREAK COLON suite { $loc, For ($2, $4, $6, $11) }
  | IF expr COLON suite { $loc, If [Some $2, $4] }
  | IF expr COLON suite elif_suite { $loc, If ((Some $2, $4) :: $5) }
  | MATCH expr COLON NL INDENT case+ DEDENT { $loc, Match ($2, $6) }
  | try_statement | with_statement | class_statement { $1 }
suite:
  | FLNE(SEMICOLON, small_statement) NL { List.concat $1 }
  | NL INDENT statement+ DEDENT { List.concat $3 }
elif_suite:
  | ELIF expr COLON suite { [Some $2, $4] }
  | ELSE COLON suite { [None, $3] }
  | ELIF expr COLON suite elif_suite { (Some $2, $4) :: $5 }
case:
  | CASE pipe_expr COLON suite { { pattern = flat_pipe $2; guard = None; pat_stmts = $4 } }
  | CASE pipe_expr IF bool_expr COLON suite { { pattern = flat_pipe $2; guard = Some $4; pat_stmts = $6 } }
  /* | CASE case_or AS ID COLON suite { ($loc, BoundPattern ($4, $2)), $6 } */

import_statement:
  | IMPORT FLNE(COMMA, import_term)
    { $2 |> List.map (fun (pos, ((imp_from, imp_args, imp_ret), imp_as)) ->
                        pos, Import { imp_from; imp_what = None; imp_args; imp_ret; imp_as; imp_dots = 0 }) }
  | FROM import_from_expr IMPORT MUL
    { let imp_dots, imp_from = $2 in
      [$loc, Import { imp_from;
                      imp_what = Some ($loc($4), Id "*");
                      imp_args = [];
                      imp_ret = None;
                      imp_as = None;
                      imp_dots}] }
  | FROM import_from_expr IMPORT FLNE(COMMA, import_term)
    { let imp_dots, imp_from = $2 in
      $4 |> List.map (fun (pos, ((i, imp_args, imp_ret), imp_as)) ->
                        pos, Import { imp_from; imp_what = Some i; imp_args; imp_ret; imp_as; imp_dots }) }
import_from_expr:
  | expr { (0, $1) }
  | DOT+ { (List.length $1, ($loc, Id "")) }
  | DOT+ expr { (List.length $1, $2) }
import_term:
  | import_lterm { $loc, ($1, None) }
  | import_lterm AS ID { $loc, ($1, Some $3) }
import_lterm:
  | dot_term { ($1, [], None) }
  | dot_term LP FL(COMMA, import_param) RP func_ret_type? { $1, $3, $5 }
import_param: expr { $loc, { name = ""; typ = Some $1; default = None } }
dot_term: ID { $loc, Id $1 } | dot_term DOT ID { $loc, Dot ($1, $3) }

assign_statement:
  | expr aug_eq expr { [$loc, Assign ($1, Some ($loc, Binary ($1, String.sub $2 0 (String.length $2 - 1), $3, true)), None)] }
  | ID COLON expr EQ expr { [$loc, Assign (($loc($1), Id $1), Some $5, Some $3)] }
  | expr_list EQ separated_nonempty_list(EQ, expr_list)
    { let all = List.map (function [l] -> l | l -> $loc, Tuple l) (List.rev ($1 :: $3)) in
      List.rev @@ List.map (fun i -> $loc, Assign (i, Some (List.hd all), None)) (List.tl all) }
%inline aug_eq: PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ | LSHEQ | RSHEQ | ANDEQ | OREQ | XOREQ { $1 }

try_statement: TRY COLON suite catch* finally? { $loc, Try ($3, $4, opt_val $5 []) }
catch:
  /* TODO: except (RuntimeError, TypeError, NameError) */
  | EXCEPT COLON suite { $loc, { exc = None; var = None; stmts = $3 } }
  | EXCEPT expr COLON suite { $loc, { exc = Some $2; var = None; stmts = $4 } }
  | EXCEPT expr AS ID COLON suite { $loc, { exc = Some $2; var = Some $4; stmts = $6 } }
finally: FINALLY COLON suite { $3 }

with_statement: WITH FLNE(COMMA, with_clause) COLON suite { $loc, With ($2, $4) }
with_clause: expr { $1, None } | expr AS ID { $1, Some $3 }

%public decorator(X):
  | decorator_term* X { $1 } /* AT dot_term NL | AT dot_term LP FL(COMMA, expr) RP NL */
decorator_term:
  | AT ID NL { $loc, $2 }

func_statement:
  | func_def COLON EXTERN { [$loc, Function { $1 with fn_stmts = [$loc, Expr ($loc, String ("", $3))] }] }
  | func_def COLON suite { [$loc, Function { $1 with fn_stmts = $3 }] }
func_def:
  | decorator(DEF) ID generic_list? LP FL(COMMA, typed_param) RP func_ret_type?
    { { fn_name = $2; fn_rettyp = $7; fn_generics = opt_val $3 []; fn_args = $5; fn_stmts = []; fn_attrs = $1 } }
typed_param: ID param_type? default_val? { $loc, { name = $1; typ = $2; default = $3 } }
generic_list: LS FLNE(COMMA, typed_param) RS { $2 }
default_val: EQ expr { $2 }
param_type: COLON expr { $2 }
func_ret_type: OF expr { $2 }

class_statement: cls { $1 }
cls: decorator(CLASS) cls_body { $loc, Class { $2 with attrs = $1 } }
cls_body:
  | ID generic_list? COLON NL INDENT dataclass_member+ DEDENT
    { let args = List.rev @@ List.fold_left
        (fun acc i -> match i with
          | Some (pos, Assign ((_, Id name), default, typ)) -> (pos, { name; typ; default }) :: acc
          | _ -> acc) [] $6
      in
      let members = List.rev @@ List.fold_left
        (fun acc i -> match i with Some (_, Assign _) | None -> acc | Some p -> p :: acc) [] $6
      in
      { class_name = $1; generics = opt_val $2 []; args; members; attrs = [] } }
dataclass_member:
  | NL | PASS NL { None }
  | ID COLON expr NL
    { Some ($loc, Assign (($loc, Id $1), None, Some $3)) }
  | ID COLON expr EQ expr NL
    { Some ($loc, Assign (($loc, Id $1), Some $5, Some $3)) }
  | string NL { Some ($loc, Expr ($loc, String $1)) }
  | func_statement { Some (List.hd $1) }
  | class_statement { Some $1 }

/* typ:
  | type_head NL { $loc, Type (snd $1) }
  | type_head COLON NL INDENT class_member+ DEDENT { $loc, Type { (snd $1) with members = filter_opt $5 } }
type_head:
  | decorator(TYPE) ID generic_list? LP FL(COMMA, typed_param) RP
    { $loc, { class_name = $2; generics = opt_val $3 []; args = $5; members = []; attrs = $1 } } */
