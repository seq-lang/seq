/* 786 */

%{
open Ast
%}

%token <int> INT
%token <float> FLOAT
%token <string> ID
%token LPAREN RPAREN EQ
%token EOL EOF  
%token LET MODULE
%token<string> ADD SUB MUL PIPE BRANCH

%nonassoc EQ
%left PIPE 
%left BRANCH
%left ADD SUB
%left MUL

%start <Ast.ast> program
%%

program:
  | s = statement; EOF { s }
  | EOF { Eof }
  ;

statement:
  /* | MODULE; p = ID; s = ID; EOL { Module(p, s) }  */
  | LET; p = ID; EQ; e = expr; EOL { Definition(Prototype(p, []), [e]) }
  | e = expr; EOL { Expr e }
  | EOL { Eof }
  ;

expr: 
  | n = simple_expr { n } 
  | id = ID; args = arg_list { Identifier(id, args) }
  | e1 = expr; op = ADD; e2 = expr { Binary(e1, op, e2) }
  | e1 = expr; op = SUB; e2 = expr { Binary(e1, op, e2) }
  | e1 = expr; op = MUL; e2 = expr { Binary(e1, op, e2) }
  | e1 = expr; op = PIPE; e2 = expr { Binary(e1, op, e2) }
  | e1 = expr; op = BRANCH; e2 = expr { Binary(e1, op, e2) }
  | LPAREN; e = expr; RPAREN { e }
  ;
simple_expr:
  | n = INT { Int n } 
  | n = FLOAT { Float n } 
  | id = ID { Identifier(id, []) }
  ;
arg_list:
  | h = simple_expr 
  | LPAREN; h = expr; RPAREN { [h] } 
  | h = simple_expr; r = arg_list
  | LPAREN; h = expr; RPAREN; r = arg_list { h::r }
  ;





