/* 786 */

%{
open Ast
%}

%token <int> INT
%token <float> FLOAT
%token <string> STRING ID
%token LPAREN RPAREN EQ
%token EOF  
%token LET
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
  | LET; p = ID; EQ; e = expr; EOF { Definition(Prototype(p, []), [e]) }
  | e = expr; EOF { Expr e }
  ;

expr: 
  | n = simple_expr { n } 
  | id = ID; args = arg_list { Identifier(id, args) }
  | e1 = expr; o = op; e2 = expr { Binary(e1, o, e2) }
  | LPAREN; e = expr; RPAREN { e }
  ;
%inline op:
  | ADD | SUB | MUL | PIPE | BRANCH { $1 }
  ;
simple_expr:
  | n = INT { Int n } 
  | n = FLOAT { Float n } 
  | n = STRING { String n } 
  | id = ID { Identifier(id, []) }
  ;
arg_list:
  | h = simple_expr 
  | LPAREN; h = expr; RPAREN { [h] } 
  | h = simple_expr; r = arg_list
  | LPAREN; h = expr; RPAREN; r = arg_list { h::r }
  ;





