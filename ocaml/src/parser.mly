/* 786 */

%{
  open Ast
  open Core

  let flat x = match x with
    | [] -> raise_s [%message "flat function failed unexpectedly"] 
    | h::[] -> h 
    | h::el -> Pipe (h::el)

  let noimp s = raise (NotImplentedError ("Not yet implemented: " ^ s))
%}

%token <int * Ast.pos_t>             INT
%token <float * Ast.pos_t>           FLOAT
%token <string * Ast.pos_t>          STRING ID GENERIC
%token <string * Ast.pos_t>          REGEX SEQ
%token <string * string * Ast.pos_t> EXTERN

/* blocks */
%token <Ast.pos_t> INDENT 
%token <Ast.pos_t> DEDENT
%token <Ast.pos_t> EOF
%token <Ast.pos_t> NL        /* \n */
%token <Ast.pos_t> DOT       /* . */
%token <Ast.pos_t> COLON     /* : */
%token <Ast.pos_t> SEMICOLON /* ; */
%token <Ast.pos_t> AT        /* @ */
%token <Ast.pos_t> COMMA     /* , */
%token <Ast.pos_t> OF        /* -> */

/* parentheses */
%token <Ast.pos_t> LP RP     /* ( ) parentheses */
%token <Ast.pos_t> LS RS     /* [ ] squares */
%token <Ast.pos_t> LB RB     /* { } braces */

/* keywords */
%token <Ast.pos_t> FOR IN WHILE CONTINUE BREAK        /* loops */
%token <Ast.pos_t> IF ELSE ELIF MATCH CASE AS DEFAULT /* conditionals */
%token <Ast.pos_t> DEF RETURN YIELD LAMBDA            /* functions */
%token <Ast.pos_t> TYPE CLASS TYPEOF EXTEND           /* types */
%token <Ast.pos_t> IMPORT FROM GLOBAL                 /* variables */
%token <Ast.pos_t> PRINT PASS ASSERT                  /* keywords */
%token <Ast.pos_t> TRUE FALSE                         /* booleans */

/* operators */
%token <Ast.pos_t>         EQ ASSGN_EQ ELLIPSIS
%token<string * Ast.pos_t> ADD SUB MUL DIV FDIV POW MOD 
%token<string * Ast.pos_t> PLUSEQ MINEQ MULEQ DIVEQ MODEQ POWEQ FDIVEQ
%token<string * Ast.pos_t> AND OR NOT
%token<string * Ast.pos_t> EEQ NEQ LESS LEQ GREAT GEQ
%token<string * Ast.pos_t> PIPE 

/* operator precedence */
%left ADD SUB
%left MUL DIV FDIV POW MOD

%start <Ast.ast> program
%%

program: /* Entry point */
  | statement+ EOF { Module $1 }

/*******************************************************/

atom: /* Basic structures: identifiers, nums/strings, tuples/list/dicts */
  | ID { Id (fst $1, snd $1) }
  | INT { Int (fst $1, snd $1) } 
  | FLOAT { Float (fst $1, snd $1) } 
  | TRUE { Bool (true, $1) }
  | FALSE { Bool (false, $1) }
  | STRING { String (fst $1, snd $1) } 
  | REGEX { 
    noimp "Regex" 
    (* Regex $1 *) }
  | SEQ { Seq $1 }
  | EXTERN { 
    noimp "Extern" 
    (* let a, b = $1 in Extern (a, b) *) }
  | tuple { $1 }
  | list | dict { $1 }
  | generic { $1 }
generic:
  | GENERIC { Generic (fst $1, snd $1) }
tuple: /* Tuples: (1, 2, 3) */
  | LP RP { Tuple ([], $1) }
  | LP test comprehension RP { 
    noimp "Generator" 
    (* Generator ($2, $3)  *)
  }
  | LP test COMMA RP { Tuple ([$2], $1)  }
  | LP test_list RP 
    { if List.length $2 > 1 then Tuple ($2, $1) else List.hd_exn $2 }
list: /* Lists: [1, 2, 3] */
  /* TODO needs trailing comma support */
  | LS RS { 
    noimp "List"
    (* List [] *) }
  | LS test comprehension RS { 
    noimp "List"
    (* ListGenerator ($2, $3) *) }
  | LS test_list RS { 
    noimp "List"
    (* List $2 *) }
dict: /* Dictionaries and sets: {1: 2, 3: 4}, {1, 2} */
  | LB RB { 
    noimp "Dict"
    (* Dict [] *) }
  | LB separated_nonempty_list(COMMA, test) RB { 
    noimp "Dict"
    (* Set $2 *) }
  | LB test comprehension RB { 
    noimp "Dict"
    (* SetGenerator ($2, $3) *) }
  | LB dictitem comprehension RB { 
    noimp "Dict"
    (* DictGenerator ($2, $3) *) }
  | LB separated_nonempty_list(COMMA, dictitem) RB { 
    noimp "Dict"
    (* Dict $2 *) }
dictitem: 
  | test COLON test { ($1, $3) }

comprehension:
  | FOR separated_nonempty_list(COMMA, expr) 
    IN separated_nonempty_list(COMMA, pipe_test) 
    comprehension? 
    { noimp "Comprehension"
    (* Comprehension ($2, List.map ~f:flat $4, $5) *) }
  | FOR separated_nonempty_list(COMMA, expr) 
    IN separated_nonempty_list(COMMA, pipe_test) 
    IF pipe_test 
    { noimp "Comprehension"
    (* Comprehension ($2, List.map ~f:flat $4, Some (ComprehensionIf (flat $6))) *) }

/*******************************************************/

test: /* General expression: 5 <= p.x[1:2:3] - 16, 5 if x else y, lambda y: y+3 */
  | pipe_test { flat $1 }
  | ifc = pipe_test; IF cnd = pipe_test; ELSE elc = test 
    { IfExpr (flat cnd, flat ifc, elc) }
  | TYPEOF LP test RP { TypeOf ($3, $1) }
  /* TODO: shift/reduce conflict
  | LAMBDA separated_list(COMMA, param) COLON test { 
    noimp "Lambda"
    Lambda ($2, $4) 
  } */
test_list: 
  | separated_nonempty_list(COMMA, test) { $1 }
pipe_test: /* Pipe operator: a, a |> b */
  | or_test { [$1] }
  | or_test PIPE pipe_test { $1::$3 }
or_test: /* OR operator: a, a or b */
  | and_test { $1 }
  | and_test OR or_test { Binary ($1, $2, $3) }
and_test: /* AND operator: a, a and b */
  | not_test { $1 }
  | not_test AND and_test { Binary ($1, $2, $3) } 
not_test: /* General comparison: a, not a, a < 5 */
  | expr { $1 }
  | NOT not_test { Unary (("!", snd $1), $2) }
  | expr cond_op not_test { Binary ($1, $2, $3) }
%inline cond_op:
  /* TODO: in, is in, is not in, not in, not */
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ { $1 }
expr_term: /* Expression term: 4, a(4), a[5], a.x, p */
  | atom { $1 }
  | expr_term LP; args = separated_list(COMMA, call_term); RP 
    { Call ($1, args) }
  | expr_term LS separated_nonempty_list(COMMA, sub) RS
    /* TODO: tuple index */
    { Index ($1, $3) }
  | expr_term DOT ID 
    { Dot ($1, (fst $3, snd $3)) }
call_term:
	| ELLIPSIS { Ellipsis }
	| test { $1 }
expr: /* General arithmetic: 4, 5 + p */
  | expr_term { $1 }
  | ADD expr_term 
  | SUB expr_term { Unary($1, $2) }
  | expr bin_op expr { Binary ($1, $2, $3) }
sub: /* Subscripts: ..., a, 1:2, 1::3 */
  /* TODO: support args/kwargs? */
  | ELLIPSIS { 
    noimp "Ellipsis" 
    (* Ellipsis *) }
  | test { $1 }
  | test? COLON test? { Slice ($1, $3, None, $2) }
  | test? COLON test? COLON test? { Slice ($1, $3, $5, $2) }
%inline bin_op: 
  /* TODO: bit shift ops and ~ */
  /* TODO: unary op */
  | ADD | SUB | MUL | DIV | FDIV | MOD | POW { $1 }  

/*******************************************************/

statement: /* Statements */
  /* TODO: try/except, with */
  /* n.b. for/while does not support else */
  | separated_nonempty_list(SEMICOLON, small_statement) NL { 
    if List.length $1 = 1 then List.hd_exn $1 else Statements $1 }
  | WHILE test COLON suite { While ($2, $4, $1) }
  | FOR expr IN test COLON suite 
    { For ($2, $4, $6, $1) }
  | IF test COLON suite { If ([(Some $2, $4, $1)]) }
  | IF test COLON suite; rest = elif_suite { If ((Some $2, $4, $1)::rest) }
  | MATCH test COLON NL INDENT case_suite DEDENT { Match ($2, $6, $1) }
  | func_statement 
  | class_statement
  | extend_statement
    { $1 }
  | NL { Pass $1 }
small_statement: /* Simple one-line statements: 5+3, print x */
  /* TODO del, exec/eval?,  */
  | expr_statement { $1 }
  | import_statement { $1 }
  | PRINT test_list { Print ($2, $1) }
  | PASS { Pass $1 }
  | BREAK { Break $1 }
  | CONTINUE { Continue $1 }
  | RETURN test_list { 
    (*TODO: tuples *) 
    Return (List.hd_exn $2, $1) }
  | YIELD test_list { 
    (*TODO: tuples *) 
    Yield (List.hd_exn $2, $1) }
  | TYPE ID LP separated_list(COMMA, typed_param) RP { Type ((fst $2, snd $2), $4, $1) }
  | GLOBAL separated_nonempty_list(COMMA, ID) 
    { noimp "Global"
    (* Global (List.map ~f:(fun x -> Id x) $2) *) }
  | ASSERT test_list { 
    noimp "Assert" 
    (* Assert $2 *) }
expr_statement: /* Expression statement: a + 3 - 5 */
  | test_list { 
      (*assert List.length $1 = 1;*)
      Exprs (List.hd_exn $1)
    }
  /* TODO: https://www.python.org/dev/peps/pep-3132/ */
  | test aug_eq test_list { 
    (* TODO tuple assignment *)
    let op, pos = fst $2, snd $2 in
    let op = String.sub op ~pos:0 ~len:(String.length op - 1) in
    Assign ($1, Binary($1, (op, pos), List.hd_exn $3), false)
  }
   /* TODO: a = b = c = d = ... separated_nonempty_list(EQ, test_list) {  */
  | test EQ test_list { 
    Assign ($1, List.hd_exn $3, false) 
  }
  | test ASSGN_EQ test_list { 
    Assign ($1, List.hd_exn $3, true) 
  }

%inline aug_eq: 
  /* TODO: bit shift ops */
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ { $1 }
suite: /* Indentation blocks */
  | separated_nonempty_list(SEMICOLON, small_statement) NL { $1 }
  | NL INDENT statement+ DEDENT { $3 }
elif_suite:
  | ELIF test COLON suite { [(Some $2, $4, $1)] }
  | ELSE COLON suite { [(None, $3, $1)] }
  | ELIF test COLON suite; rest = elif_suite { (Some $2, $4, $1)::rest }
case_suite:
  | DEFAULT COLON suite { [(None, None, $3, $1)] }
  | case { [$1] }
  | case; rest = case_suite { $1::rest }
case:
  | CASE test COLON suite { (Some $2, None, $4, $1) }
  | CASE test AS ID COLON suite { (Some $2, Some (fst $4, snd $4), $6, $1) }
import_statement:
  | FROM dotted_name IMPORT MUL { 
    noimp "Import" 
    (* ImportFrom ($2, None) *) }
  | FROM dotted_name IMPORT separated_list(COMMA, import_as) { 
    noimp "Import" 
    (* ImportFrom ($2, Some $4) *) }
  | IMPORT separated_list(COMMA, import_as) 
    { Import ($2, $1) }
import_as:
  | ID { ((fst $1, snd $1), None) }
  | ID AS ID { ((fst $1, snd $1), Some (fst $3, snd $3)) }

/*******************************************************/

func_statement:
  | func { $1 }
  | decorator+ func { 
    noimp "decorator"
    (* DecoratedFunction ($1, $2) *) }
decorator:
  | AT dotted_name NL { 
    noimp "decorator"
    (* Decorator ($2, []) *) }
  | AT dotted_name LP separated_list(COMMA, test) RP NL { 
    noimp "decorator"
    (* Decorator ($2, $4) *) }
dotted_name:
  | ID { Id (fst $1, snd $1) }
  | dotted_name DOT ID { Dot ($1, (fst $3, snd $3)) }

generic_type_list:
   | LS; separated_nonempty_list(COMMA, generic); RS { $2 }
func: 
  | DEF; n = ID;
    intypes = generic_type_list?
    LP params = separated_list(COMMA, func_param); RP
    ret = func_ret_type?
    COLON;
    s = suite 
    { let intypes = match intypes with None -> [] | Some x -> x in
      Function (Arg(n, ret), intypes, params, s, $1) }
func_ret_type:
  | OF; test { $2 }
func_param:
  /* TODO tuple params--- are they really needed? */
  | typed_param { $1 }
  | ID EQ test { 
    noimp "NamedArg"
    (*NamedArg ($1, $3)*) }
typed_param:
  | ID param_type? { Arg ((fst $1, snd $1), $2) }
param_type:
  | COLON test { $2 }

class_statement:
  | CLASS ; n = ID;
    intypes = generic_type_list?
    LP; mems = separated_list(COMMA, typed_param) RP;
    COLON NL; 
    fns = class_members
    { let intypes = Option.value intypes ~default:[] in
      Class ((fst n, snd n), intypes, mems, fns, $1) }
class_members:
  | PASS { [] }
  | INDENT func_statement+ DEDENT { $2 } 
extend_statement:
  | EXTEND ; n = ID; COLON NL; 
    fns = class_members
    { Extend ((fst n, snd n), fns, $1) }

