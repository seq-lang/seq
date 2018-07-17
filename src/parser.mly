/* 786 */

%{
  open Ast
%}

%token <int> INT
%token <float> FLOAT
%token <string> STRING ID
%token <string * string> EXTERN

/* blocks */
%token INDENT, DEDENT
%token EOF, NL
%token DOT   /* . */
%token COLON /* : */
%token COMMA /* , */

/* parentheses */
%token LP RP /* ( ) parentheses */
%token LS RS /* [ ] squares */
%token LB RB /* { } braces */

/* keywords */
%token FOR IN 
%token CONTINUE BREAK
%token IF ELSE ELIF
%token MATCH CASE AS
%token DEF RETURN YIELD
%token PRINT PASS

/* booleans */
%token TRUE FALSE

/* operators */
%token EQ
%token<string> ADD SUB MUL DIV 
%token<string> AND OR NOT
%token<string> LESS LEQ GREAT GEQ
%token<string> EEQ NEQ
%token<string> PIPE 

/* operator precedence */
%nonassoc EQ
%left PIPE 
%left AND OR
%left LESS GREAT LEQ GEQ EEQ NEQ
%left ADD SUB
%left MUL DIV

%start <Ast.ast> program
%%

/* the grammar spec */

program: 
  | EOF { Module [] }
  | s = statement_list; EOF { Module s }
  ;

statement:
  /* function */
  | DEF; n = ID; LP; args = arg_list; RP; COLON; NL;
    INDENT; s = statement_list; DEDENT 
    { Function(n, args, s) }
  | RETURN; e = expr; NL 
    { Return(flatten e) }
  | YIELD; e = expr; NL 
    { Yield(flatten e) }
  /* for */
  | FOR; i = ID; IN; it = expr; COLON; NL;
    INDENT; s = statement_list; DEDENT
    { For(i, flatten it, s) }
  | BREAK; NL 
    { Break }
  | CONTINUE; NL 
    { Continue }
  /* if then else */
  | IF; cond = expr; COLON; NL;
    INDENT; s1 = statement_list; DEDENT
    ELSE; COLON; NL;
    INDENT; s2 = statement_list; DEDENT
    { IfElse(flatten cond, s1, s2) }
  /* if then */
  | IF; cond = expr; COLON; NL;
    INDENT; s = statement_list; DEDENT
    { IfElse(flatten cond, s, []) }
  /* match case */
  | MATCH; what = expr; COLON; NL;
    INDENT; c = case_list; DEDENT
    { Match(flatten what, c) }
  /* x = expr */
  | p = ID; EQ; e = expr; NL
    { Assign(p, flatten e) }
  /* print x */
  | PRINT; e = expr_list; NL
    { Print(e) }
  | PRINT; NL
    { Print([]) }
  /* expr */
  | e = expr; NL
    { Expr (flatten e) }
  | PASS; NL
    { Pass }
  ;

case:
  | CASE; c = expr; AS; a = ID; COLON; NL;
    INDENT; s = statement_list; DEDENT 
    { Case(flatten c, Some a, s) } 
  | CASE; c = expr; COLON; NL;
    INDENT; s = statement_list; DEDENT 
    { Case(flatten c, None, s) } 

case_list:
  | c = case { [c] }
  | c = case; r = case_list { c::r }
  ;

statement_list:
  | h = statement { [h] } 
  | h = statement; r = statement_list { h::r }
  ;

arg_list:
  | { [] }
  | h = ID { [h] } 
  | h = ID; COMMA; r = arg_list { h::r }
  ;

expr_list:
  | h = expr { [flatten h] } 
  | h = expr; COMMA; r = expr_list { (flatten h)::r }
  ;

expr:
  /* 1, true, "hehe", x, -5.3 */
  | n = simple_expr 
    { n } 
  /* `extern` */
  | e = EXTERN
    { let (l, v) = e in 
      Extern(l, v) }
  /* f[a] */
  | a = ID; LS; e = expr; RS
    { Index(a, flatten e) }
  /* f(a, b) */
  | id = ID; LP; args = expr_list; RP 
    { Call(id, args) }
  | id = ID; LP; RP 
    { Call(id, []) }
  /* [a, b] */
  | LS; args = expr_list; RS 
    { List(args) }
  | LS; RS 
    { List([]) }
  /* e1 + e2 */
  | e1 = expr; o = bop; e2 = expr 
    { Binary(e1, o, e2) }
  /* print in the pipe--- reduce conflict */
  /* | PRINT
    { Print([]) } */
  /* a if x else b */
  /* | e1 = expr; IF; cond = expr; ELSE; e2 = expr
      { IfExpr(cond, e1, e2) } */
  /* (x) */
  | LP; e = expr; RP 
    { flatten e }
  ;
%inline bop:
  | AND | OR
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ
  | ADD | SUB | MUL | DIV 
  | PIPE 
    { $1 }
  ;

simple_expr:
  | n = INT { Int n } 
  | n = FLOAT { Float n } 
  | n = TRUE { Bool true }
  | n = FALSE { Bool false }
  | n = STRING { String n } 
  | id = ID { Ident(id) }
  ;
