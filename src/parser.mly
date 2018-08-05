/* 786 */

%[@trace true]

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
%token FOR IN WHILE
%token CONTINUE BREAK
%token IF ELSE ELIF
%token MATCH CASE AS DEFAULT
%token DEF OF RETURN YIELD
%token PRINT PASS
%token TYPE

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
  | s = statement*; EOF { Module s }
  ;

/*******************************************************/

statement:
/* function */
  | DEF; n = ID; LP; args = separated_list(COMMA, defarg); RP; OF; t = ID; COLON; NL;
    INDENT; s = statement+; DEDENT 
    { Function((n, Type t), args, s) }
  | DEF; n = ID; LP; args = separated_list(COMMA, defarg); RP; COLON; NL;
    INDENT; s = statement+; DEDENT 
    { Function((n, Auto), args, s) }
  | RETURN; e = expr; NL 
    { Return(flatten e) }
  | YIELD; e = expr; NL 
    { Yield(flatten e) }
/* for */
  | FOR; i = ID; IN; it = expr; COLON; NL;
    INDENT; s = statement+; DEDENT
    { For(i, flatten it, s) }
  | BREAK; NL 
    { Break }
  | CONTINUE; NL 
    { Continue }
/* while */
  | WHILE; it = expr; COLON; NL;
    INDENT; s = statement+; DEDENT
    { While(flatten it, s) }
/* if elif else */
  | IF; cond = expr; COLON; NL;
    INDENT; s = statement+; DEDENT
    { If [(flatten cond, s)] }
  | IF; cond = expr; COLON; NL;
    INDENT; s = statement+; DEDENT
    e = elifs;
    { If((flatten cond, s)::e) }
/* match case */
  | MATCH; what = expr; COLON; NL;
    INDENT; c = case+; DEDENT
    { Match(flatten what, c) }
/* x = expr */
  | p = ID; EQ; e = expr; NL
    { Assign(p, flatten e) }
/* print x */
  | PRINT; e = separated_list(COMMA, expr); NL
    { Print(e) }
/* expr */
  | e = expr; NL
    { Expr (flatten e) }
/* pass */
  | PASS; NL
  | NL
    { Pass }
  ;

/* */
atom := identifier | literal | enclosure
enclosure ::=  parenth_form | list_display
               | generator_expression | dict_display | set_display
               | string_conversion | yield_atom
primary ::=  atom | attributeref | subscription | slicing | call
attributeref ::=  primary "." identifier


/* slicing */

/* */

primary ::=  atom | attributeref | subscription | slicing | call
slicing          ::=  simple_slicing | extended_slicing
simple_slicing   ::=  primary "[" short_slice "]"
extended_slicing ::=  primary "[" slice_list "]"
slice_list       ::=  slice_item ("," slice_item)* [","]
slice_item       ::=  expression | proper_slice | ellipsis
proper_slice     ::=  short_slice | long_slice
short_slice      ::=  [lower_bound] ":" [upper_bound]
long_slice       ::=  short_slice ":" [stride]
lower_bound      ::=  expression
upper_bound      ::=  expression
stride           ::=  expression
ellipsis         ::=  "..."
augmented_assignment_stmt ::=  augtarget augop (expression_list | yield_expression)
augtarget                 ::=  identifier | attributeref | subscription | slicing
augop                     ::=  "+=" | "-=" | "*=" | "/=" | "//=" | "%=" | "**="
                               | ">>=" | "<<=" | "&=" | "^=" | "|="


simple_stmt:
  | expr
  | ASSERT; e = expr { Assert(flatten e) }
  | (i = ID | )

/*******************************************************/

case:
  | CASE; c = expr; AS; a = ID; COLON; NL;
    INDENT; s = statement+; DEDENT 
    { (flatten c, Some a, s) } 
  | CASE; c = expr; COLON; NL;
    INDENT; s = statement+; DEDENT 
    { (flatten c, None, s) } 
  | DEFAULT; COLON; NL;
    INDENT; s = statement+; DEDENT 
    { (Default, None, s) } 
  ;

elifs:
  | ELIF; e = expr; COLON; NL;
    INDENT; s = statement+; DEDENT;  
    { [(flatten e, s)] }
  | ELSE; COLON; NL;
    INDENT; s = statement+; DEDENT;  
    { [(Bool true, s)] }
  | ELIF; e = expr; COLON; NL;
    INDENT; s = statement+; DEDENT;  
    r = elifs
    { (flatten e, s)::r }
  ;

defarg:
  | k = ID; EQ; v = expr { KeyValArg((k, Auto), flatten v) }
  | h = ID; OF; t = ID { Arg(h, Type t) } 
  | v = ID { Arg(v, Auto) }
  ;

/*******************************************************/

callarg:
  | k = ID; EQ; v = expr { NamedArg(k, flatten v) }
  | v = expr { ExprArg(flatten v) }
  ;

dict:
  | k = expr; COLON; v = expr { (k, v) } 
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
  /* f(a, b, c=_c) */
  | id = ID; LP; args = separated_list(COMMA, callarg); RP 
    { Call(id, args) }
  /* [a, b] */
  | LS; args = separated_list(COMMA, expr); RS 
    { List(args) }
  /* {a: b, c: d} */
  | LB; args = separated_list(COMMA, dict); RB
    { Dict(args) }
  /* a as x */
  | LP; a = ID; AS; t = ID; RP
    { As(a, t) }
  /* e1 + e2 */
  | e1 = expr; o = bop; e2 = expr 
    { Binary(e1, o, e2) }
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

/* */
expression_list:
  | el = separated_list(COMMA, expr); COMMA?
    { el }
  ;
target_list:
  | tl = separated_list(COMMA, target); COMMA?
    { tl }
  ;
assignment_stmt: /* assignment a = x */
  | sl = separated_list(EQ, target_list); el = expression_list
    { Assign(sl, el) }
  ;
target: /* target (lhs) of assignment */
  | i = ID { Ident(i) }
  | LP; tl = target_list; RP { tl }
  | LB; tl = target_list; RB { tl }
  | attributeref | subscription | slicing { $0 }
  ;
attributeref: /* dot notation: a.x */
  | p = primary; DOT; i = ID 
    { Dot(p, i) }
  ;
subscription: /* select element: a[x] */
  | p = primary; LB; el = expression_list; RB
    { Index(p, el) }
  ;
slicing:
  | p = primary; LB; s = separated_nonempty_list(COMMA, slice); COMMA?; RB
    { SliceIndex(p, s) }
  ;
slice:
  | e = expr { e }
  | l = expr?; COLON; h = expr? 
    { Slice(l, h, None) }
  | l = expr?; COLON; h = expr?; COLON; s = expr? 
    { Slice(l, h, s) }  
  | ELLIPSIS  { Ellipsis }
  ;
primary:
  | atom | attributeref | subscription | slicing | call 
    { $0 }
  ;
atom:
  /* identifiers and literals */
  | i = ID { Ident(i) }
  | literal | enclosure { $0 }
  ;
enclosure:
  /* tuples */
  | LP; el = expression_list; RP
    { el }
  /* (expr) */
  | LP; e = expr; RP 
    { flatten e }
  /* lists */
  | LS; el = expression_list; RS 
    { List(el) }
  | LS; c = list_comprehension; RS 
    { List(c) }
  /* dicts */
  | LB; el = separated_list(COMMA, dict); COMMA?; RB
    { Dict(el) }
  | LB; c = dict_comprehension; RB
    { Dict(c) }
  /* sets */
  | LB; el = expression_list; RB
    { Set(el) }
  | LB; c = comprehension; RB
    { Set(c) }
  /* string backquotes are repurposed */
  /* yield expressions are not supported */
  ;

literal:
  | n = INT { Int n } 
  | n = FLOAT { Float n } 
  | n = TRUE { Bool true }
  | n = FALSE { Bool false }
  | n = STRING { String n } 
  /* needs support for single quote strings and seqs */
  ;
