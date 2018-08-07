/* 786 */

%[@trace true]

%{
  open Ast
  open Core

  let flat x = match x with
    | [] -> raise_s [%message "flat function failed unexpectedly" (x: expr list)]
    | h::[] -> h 
    | h::el -> Pipe (h::el)
%}

%token <int> INT
%token <float> FLOAT
%token <string> STRING ID
%token <string> REGEX SEQ
%token <string * string> EXTERN

/* blocks */
%token INDENT, DEDENT
%token EOF, NL   /* EOF, \n */
%token DOT       /* . */
%token COLON     /* : */
%token SEMICOLON /* ; */
%token AT        /* @ */
%token COMMA     /* , */

/* parentheses */
%token LP RP /* ( ) parentheses */
%token LS RS /* [ ] squares */
%token LB RB /* { } braces */

/* keywords */
%token FOR IN WHILE
%token CONTINUE BREAK
%token IF ELSE ELIF
%token MATCH CASE AS DEFAULT
%token DEF OF RETURN YIELD
%token PRINT PASS IMPORT FROM
%token TYPE LAMBDA ASSERT GLOBAL

/* booleans */
%token TRUE FALSE

/* operators */
%token EQ ELLIPSIS
%token<string> ADD SUB MUL DIV FDIV POW MOD 
%token<string> PLUSEQ MINEQ MULEQ DIVEQ MODEQ POWEQ FDIVEQ
%token<string> AND OR NOT
%token<string> LESS LEQ GREAT GEQ
%token<string> EEQ NEQ
%token<string> PIPE 

/* operator precedence */
%left ADD SUB
%left MUL DIV FDIV POW MOD

%start <Ast.ast> program
%%

program: /* Entry point */
  | statement+ EOF { Module $1 }

/*******************************************************/

atom: /* Basic structures: identifiers, nums/strings, tuples/list/dicts */
  | ID { Id $1 }
  | INT { Int $1 } 
  | FLOAT { Float $1 } 
  | TRUE { Bool true}
  | FALSE { Bool false }
  | STRING { String $1 } 
  | REGEX { Regex $1 } 
  | SEQ { Seq $1 } 
  | EXTERN { let a, b = $1 in Extern (a, b) }
  | tuple | list | dict { $1 }
tuple: /* Tuples: (1, 2, 3) */
  | LP RP { Tuple [] }
  | LP test comprehension RP { Generator ($2, $3) }
  | LP test COMMA RP { Tuple [$2] }
  | LP test_list RP { Tuple $2 }
list: /* Lists: [1, 2, 3] */
  /* TODO needs trailing comma support */
  | LS RS { List [] }
  | LS test comprehension RS { ListGenerator ($2, $3) }
  | LS test_list RS { List $2 }
dict: /* Dictionaries and sets: {1: 2, 3: 4}, {1, 2} */
  | LB RB { Dict [] }
  | LB separated_nonempty_list(COMMA, test) RB { Set $2 }
  | LB test comprehension RB { SetGenerator ($2, $3) }
  | LB dictitem comprehension RB { DictGenerator ($2, $3) }
  | LB separated_nonempty_list(COMMA, dictitem) RB { Dict $2 }
dictitem: 
  | test COLON test { ($1, $3) }

comprehension:
  | FOR separated_nonempty_list(COMMA, expr) 
    IN separated_nonempty_list(COMMA, pipe_test) 
    comprehension? 
    { Comprehension ($2, List.map ~f:flat $4, $5) }
  | FOR separated_nonempty_list(COMMA, expr) 
    IN separated_nonempty_list(COMMA, pipe_test) 
    IF pipe_test 
    { Comprehension ($2, List.map ~f:flat $4, Some (ComprehensionIf (flat $6))) }

/*******************************************************/

test: /* General expression: 5 <= p.x[1:2:3] - 16, 5 if x else y, lambda y: y+3 */
  | pipe_test { flat $1 }
  | ifc = pipe_test; IF cnd = pipe_test; ELSE elc = test 
    { IfExpr (flat cnd, flat ifc, elc) }
  | LAMBDA separated_list(COMMA, param) COLON test 
    { Lambda ($2, $4) }
test_list: 
  | separated_nonempty_list(COMMA, test) { $1 }
pipe_test: /* Pipe operator: a, a |> b */
  | or_test { [$1] }
  | or_test PIPE pipe_test { $1::$3 }
or_test: /* OR operator: a, a or b */
  | and_test { $1 }
  | and_test OR and_test { Cond ($1, $2, $3) }
and_test: /* AND operator: a, a and b */
  | not_test { $1 }
  | not_test AND not_test { Cond ($1, $2, $3) } 
not_test: /* General comparison: a, not a, a < 5 */
  | expr { $1 }
  | NOT not_test { Not $2 }
  | expr cond_op not_test { Cond ($1, $2, $3) }
%inline cond_op:
  /* TODO: in, is in, is not in, not in, not */
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ { $1 }
expr: /* General arithmetic: 4, a(4), a[5], a.x, 5 + p */
  | atom { $1 }
  | atom LP separated_nonempty_list(COMMA, arg) RP { Call ($1, $3) }
  | atom LS separated_nonempty_list(COMMA, sub) RS { Index ($1, $3) }
  | atom DOT ID { Dot ($1, Id $3) }
  | expr bin_op expr { Binary ($1, $2, $3) }
arg: /* Arguments: 5, a=3 */
  /* TODO: arguments as generators w/o parenthesis */
  /* | test comprehension { Generator ($1, $2)  } */
  | test { PlainArg $1 }
  | ID EQ test { NamedArg (Id $1, $3) }
sub: /* Subscripts: ..., a, 1:2, 1::3 */
  /* TODO: support args/kwargs? */
  | ELLIPSIS { Ellipsis }
  | test { $1 }
  | test? COLON test? { Slice ($1, $3, None) }
  | test? COLON test? COLON test? { Slice ($1, $3, $5) }
%inline bin_op: 
  /* TODO: bit shift ops and ~ */
  | ADD | SUB | MUL | DIV | FDIV | MOD | POW { $1 }  

/*******************************************************/

statement: /* Statements */
  /* TODO: try/except, with */
  /* n.b. for/while does not support else */
  | separated_nonempty_list(SEMICOLON, small_statement) NL { Statements $1 }
  | WHILE test COLON suite { While ($2, $4) }
  | FOR separated_nonempty_list(COMMA, expr) IN test_list COLON suite 
    { For ($2, $4, $6) }
  | IF test COLON suite { If [($2, $4)] }
  | IF test COLON suite; rest = elif_suite { If (($2, $4)::rest) }
  | MATCH test COLON case_suite { Match ($2, $4) }
  | func { $1 }
  | decorator+ func { DecoratedFunction ($1, $2) }
  | NL { Pass }
small_statement: /* Simple one-line statements: 5+3, print x */
  /* TODO del, exec/eval?,  */
  | expr_statement { $1 }
  | import_statement { $1 }
  | PRINT test_list { Print $2 }
  | PASS { Pass }
  | BREAK { Break }
  | CONTINUE { Continue }
  | RETURN test_list { Return $2 }
  | YIELD test_list { Yield $2 }
  | TYPE ID LP separated_list(COMMA, typed_param) RP { Type (Id $2, $4) }
  | GLOBAL separated_nonempty_list(COMMA, ID) 
    { Global (List.map ~f:(fun x -> Id x) $2) }
  | ASSERT test_list { Assert $2 }
expr_statement: /* Expression statement: a + 3 - 5 */
  /* TODO: https://www.python.org/dev/peps/pep-3132/ */
  | test_list aug_eq test_list { AssignEq ($1, $2, $3) }
   /* TODO: a = b = c = d = ... separated_nonempty_list(EQ, test_list) {  */
  | test_list EQ test_list { Assign ($1, $3) }
%inline aug_eq: 
  /* TODO: bit shift ops */
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ { $1 }
suite: /* Indentation blocks */
  | separated_nonempty_list(SEMICOLON, small_statement) NL { $1 }
  | NL INDENT statement+ DEDENT { $3 }
elif_suite:
  | ELIF test COLON suite { [($2, $4)] }
  | ELSE COLON suite { [(Bool true, $3)] }
  | ELIF test COLON suite; rest = elif_suite { ($2, $4)::rest }
case_suite:
  | DEFAULT COLON suite { [(Ellipsis, None, $3)] }
  | case { [$1] }
  | case; rest = case_suite { $1::rest }
case:
  | CASE test COLON suite { ($2, None, $4) }
  | CASE test AS ID COLON suite { ($2, Some (Id $4), $6) }
import_statement:
  | FROM dotted_name IMPORT MUL { ImportFrom ($2, None) }
  | FROM dotted_name IMPORT separated_list(COMMA, import_as) { ImportFrom ($2, Some $4) }
  | IMPORT separated_list(COMMA, import_as) { Import ($2) }
import_as:
  | dotted_name { ($1, None) }
  | dotted_name AS ID { ($1, Some (Id $3)) }

/*******************************************************/

decorator:
  | AT dotted_name NL { Decorator ($2, []) }
  | AT dotted_name LP separated_list(COMMA, arg) RP NL { Decorator ($2, $4) }
dotted_name:
  | ID { Id $1 }
  | ID DOT dotted_name { Dot (Id $1, $3) }
func: 
  | DEF; n = ID; LP a = separated_list(COMMA, param); RP COLON; 
    s = suite { Function (TypedArg (Id n, None), a, s) }
  | DEF; n = ID; LP a = separated_list(COMMA, param); RP OF; t = ID; COLON; 
    s = suite { Function (TypedArg (Id n, Some t), a, s) }
param:
  /* TODO tuple params--- are they really needed? */
  | ID { TypedArg (Id $1, None) }
  | typed_param { $1 }
  | ID EQ test { NamedArg (Id $1, $3) }
typed_param:
  | ID OF ID { TypedArg (Id $1, Some $3) }
