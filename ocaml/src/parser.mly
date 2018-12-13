(******************************************************************************
 *
 * Seq OCaml 
 * parser.mly: Menhir grammar description of Seq language 
 *
 * Author: inumanag
 *
 ******************************************************************************)

%{
  open Core
  open Ast
  open Ast.ExprNode
  open Ast.StmtNode

  let noimp s =
    failwith (sprintf "not yet implemented: %s" s)

  (* Calculates the total span of a region 
     bounded by [st] and [ed] *)
  let pos st ed =
    Ast.Pos.{ st with len = (ed.col + ed.len) - st.col }

  (* Converts list of expressions into the pipeline AST node *)
  let flat x = 
    match x with
    | _, []      -> failwith "empty pipeline expression"
    | _, h::[]   -> h
    | pos, h::el -> pos, Pipe (h::el)
%}

/* constants */
%token <Ast.Pos.t * int>    INT
%token <Ast.Pos.t * float>  FLOAT
%token <Ast.Pos.t * string> STRING ID GENERIC
%token <Ast.Pos.t * string> REGEX SEQ

/* blocks */
%token <Ast.Pos.t> INDENT 
%token <Ast.Pos.t> DEDENT
%token <Ast.Pos.t> EOF
%token <Ast.Pos.t> NL        // \n 
%token <Ast.Pos.t> DOT       // . 
%token <Ast.Pos.t> COLON     // : 
%token <Ast.Pos.t> SEMICOLON // ; 
%token <Ast.Pos.t> AT        // @ 
%token <Ast.Pos.t> COMMA     // , 
%token <Ast.Pos.t> OF        // -> 

/* parentheses */
%token <Ast.Pos.t> LP RP     // ( ) parentheses 
%token <Ast.Pos.t> LS RS     // [ ] squares 
%token <Ast.Pos.t> LB RB     // { } braces 

/* keywords */
%token <Ast.Pos.t> FOR WHILE CONTINUE BREAK           // loops 
%token <Ast.Pos.t> IF ELSE ELIF MATCH CASE AS DEFAULT // conditionals 
%token <Ast.Pos.t> DEF RETURN YIELD EXTERN LAMBDA     // functions 
%token <Ast.Pos.t> TYPE CLASS TYPEOF EXTEND           // types 
%token <Ast.Pos.t> IMPORT FROM GLOBAL IMPORT_CONTEXT  // variables 
%token <Ast.Pos.t> PRINT PASS ASSERT DEL              // keywords 
%token <Ast.Pos.t> TRUE FALSE NONE                    // booleans 
%token <Ast.Pos.t> TRY EXCEPT FINALLY THROW           // exceptions

/* operators */
%token<Ast.Pos.t>          EQ ASSGN_EQ ELLIPSIS // =, :=, ...
%token<Ast.Pos.t * string> ADD SUB MUL DIV // +, -, *, /
%token<Ast.Pos.t * string> FDIV POW MOD  // //, **, %
%token<Ast.Pos.t * string> PLUSEQ MINEQ MULEQ DIVEQ  // +=, -=, *=, /=
%token<Ast.Pos.t * string> FDIVEQ POWEQ MODEQ // //=, **=, %=
%token<Ast.Pos.t * string> AND OR NOT // and, or, not
%token<Ast.Pos.t * string> IS ISNOT IN NOTIN // is, is not, in, not in
%token<Ast.Pos.t * string> EEQ NEQ LESS LEQ GREAT GEQ // ==, !=, <, <=, >, >=
%token<Ast.Pos.t * string> PIPE // |>
%token<Ast.Pos.t * string> B_AND B_OR B_XOR B_NOT // &, |, ^, ~
%token<Ast.Pos.t * string> B_LSH B_RSH // <<, >>

/* operator precedence */
%left OR
%left AND
%left B_OR
%left B_XOR
%left B_AND
%left B_LSH B_RSH
%left ADD SUB
%left MUL DIV FDIV MOD
%left POW

/* entry rule for module */
%start <Ast.t> program
%%

/******************************************************************************
  Notes:
  - Each rule returns a tuple (position, node)
  - Position should span whole rule 
    - example: for binary expression "a+b"a position starts at "a" 
               and has length of size 3
    - Function "pos" is a helper that calculates the proper position
      given start and the end of the interval
 ******************************************************************************/

program: // Entry point 
  | statement+ EOF 
    { Module (List.concat $1) }

/******************************************************************************
  Rule groupings (least to most complex):
  1. Atoms (identifiers, numbers, terms)
  2. Expressions (operators, pipes, lambdas)
  3. Statements 
  4. Functions and classes
******************************************************************************/

/******************************************************************************
 ************                      ATOMS                     ******************
 ******************************************************************************/

// Basic structures: identifiers, nums/strings, tuples/list/dicts 
atom: 
  | NONE       { $1, Empty () }
  | ID         { fst $1, Id (snd $1) }
  | INT        { fst $1, Int (snd $1) }
  | FLOAT      { fst $1, Float (snd $1) }
  | STRING     { fst $1, String (snd $1) }
  | SEQ        { fst $1, Seq (snd $1) }
  | bool       { fst $1, Bool (snd $1) }
  | tuple      { fst $1, Tuple (snd $1) }
  | lists      { fst $1, List (snd $1) }
  | dict       { fst $1, Dict (snd $1) }
  | set        { fst $1, Set (snd $1) }
  | generic    { fst $1, Generic (snd $1) } 
  | LP expr RP { $2 }
  | tuple_gen  { $1 }
  | dict_gen   { $1 }
  | list_gen   { $1 }
  | set_gen    { $1 }
  | REGEX      { noimp "Regex" }

// Types
bool:
  | TRUE  { $1, true  }
  | FALSE { $1, false }
generic:
  | GENERIC 
    { $1 }
tuple: // Tuples: (1, 2, 3) 
  | LP RP
    { pos $1 $2, 
      [] }
  | LP expr COMMA RP
    { pos $1 $4, 
      [$2] }
  | LP expr COMMA expr_list RP
    { pos $1 $5, 
      $2 :: $4 }
lists: // Lists: [1, 2, 3] 
  // TODO needs trailing comma support 
  | LS RS
    { pos $1 $2, 
      [] }
  | LS expr_list RS
    { pos $1 $3, 
      $2 }
set:
  | LB separated_nonempty_list(COMMA, expr) RB
    { pos $1 $3, 
      $2 }
dict: // Dictionaries: {1: 2, 3: 4} 
  | LB RB
    { pos $1 $2, [] }
  | LB separated_nonempty_list(COMMA, dictitem) RB
    { pos $1 $3, 
      $2 }
dictitem:
  | expr COLON expr 
    { $1, $3 }

// Generators
tuple_gen:
  | LP expr comprehension RP
    { pos $1 $4,
      Generator ($2, $3) }
list_gen:
  | LS expr comprehension RS
    { pos $1 $4, 
      ListGenerator ($2, $3) }
set_gen:
  | LB expr comprehension RB
    { pos $1 $4, 
      SetGenerator ($2, $3) }
dict_gen:
  | LB dictitem comprehension RB
    { pos $1 $4, 
      DictGenerator ($2, $3) }

// Comprehensions
comprehension:
  | FOR separated_list(COMMA, ID) IN pipe_expr comprehension_if? comprehension?
    { let last = match $6, $5, $4 with
        | Some (p, _), _, _
        | None, Some (p, _), _
        | None, None, (p, _) -> p
      in
      pos $1 last, 
      ExprNode.{ var = List.map $2 ~f:snd; gen = flat $4; cond = $5; next = $6 } }
comprehension_if:
  | IF pipe_expr
    { let exp = flat $2 in 
      pos $1 (fst exp), 
      snd exp }

/******************************************************************************
 ************                   EXPRESSIONS                  ******************
 ******************************************************************************/

// General expression
expr:
  | pipe_expr // Pipes and other expressions
    { flat $1 }
  | ifc = pipe_expr; IF cnd = pipe_expr; ELSE elc = expr // Inline ifs
    { pos (fst ifc) (fst elc), 
      IfExpr (flat cnd, flat ifc, elc) }
  | TYPEOF LP expr RP // TypeOf call
    { pos $1 $4, 
      TypeOf $3 }
  | LAMBDA separated_list(COMMA, ID) COLON expr // Lambdas
    { pos $1 (fst $4), 
      Lambda ($2, $4) }
expr_list:
  | separated_nonempty_list(COMMA, expr) 
    { $1 }

// The following rules are defined in the order of operator precedence:
//   pipes -> booleans -> conditionals -> arithmetics

// Pipes (|>)
pipe_expr: 
  | o = bool_expr 
    { fst o, [o] }
  | bool_expr PIPE pipe_expr 
    { pos (fst $1) (fst $3), 
      $1 :: (snd $3) }

// Bool expressions 
// (binary: and, or)
bool_expr:
  | cond_expr 
    { $1 }
  | cond_expr bool_op bool_expr
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
%inline bool_op:
  | AND | OR 
  { $1 }

// Conditional operators 
// (unary: not; binary: <, <=, >, >=, ==, !=, is, is not, in, not in)
cond_expr: 
  | arith_expr 
    { $1 }
  // Unary condition
  | NOT cond_expr
    { pos (fst $1) (fst $2), 
      Unary ("!", $2) }
  // Binary condition
  | arith_expr cond_op cond_expr
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
%inline cond_op:
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ | IS | ISNOT | IN | NOTIN
  { $1 }

// Arithmetic expression 
// (unary: +, -, ~; binary: +, -, *, /, //, %, **, &, |, ^, >>, <<)
arith_expr: 
  | arith_term 
    { $1 }
  // Unary operator
  | ADD arith_term
  | SUB arith_term
  | B_NOT arith_term
    { pos (fst $1) (fst $2), 
      Unary(snd $1, $2) }
  // Binary operator
  | arith_expr arith_op arith_expr
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
%inline arith_op:
  | ADD | SUB | MUL | DIV | FDIV | MOD | POW 
  | B_AND | B_OR | B_XOR | B_LSH | B_RSH
    { $1 }

// Arithmetic terms
arith_term:
  | atom 
    { $1 }
  // Call (foo(bar))
  | arith_term LP; args = separated_list(COMMA, call_term); RP
    { pos (fst $1) $4, 
      Call ($1, args) }
  // Call with single generator parameter without parentheses
  // (foo(x for x in y))
  | arith_term LP; expr comprehension; RP
    { pos (fst $1) $5, 
      Call ($1, [ (pos $2 $5, Generator ($3, $4)) ]) }
  // Index (foo[bar])
  | arith_term LS separated_nonempty_list(COMMA, index_term) RS
    // TODO: tuple index
    { pos (fst $1) $4, 
      Index ($1, $3) }
  // Access (foo.bar)
  | arith_term DOT ID
    { pos (fst $1) (fst $3), 
      Dot ($1, snd $3) }
// Call arguments
call_term:
  | ELLIPSIS // For partial functions 
    { $1, 
      Ellipsis () }
  | expr 
    { $1 }
// Index subscripts
index_term: 
  // Normal expression
  | expr 
    { $1 }
  // Basic slice (a:b)
  | expr? COLON expr?
    { let f = Option.value_map $1 ~f:fst ~default:$2 in
      let l = Option.value_map $3 ~f:fst ~default:$2 in
      pos f l, 
      Slice ($1, $3, None) }
  // Extended slice (a:b:c)
  | expr? COLON expr? COLON expr?
    { let f = Option.value_map $1 ~f:fst ~default:$2 in
      let l = Option.value_map $5 ~f:fst ~default:$4 in
      pos f l, 
      Slice ($1, $3, $5) }

/******************************************************************************
 ************                   STATEMENTS                   ******************
 ******************************************************************************/

// General statements
statement: 
  // List of small statements optionally separated by ;
  | separated_nonempty_list(SEMICOLON, small_statement) NL
    { $1 }
  // Empty statement
  | NL
    {[ $1, 
       Pass () ]}
  // Loops
  | WHILE expr COLON suite
    {[ pos $1 $3, 
       While ($2, $4) ]}
  | FOR separated_list(COMMA, ID) IN expr COLON suite
    {[ pos $1 $5, 
       For (List.map $2 ~f:snd, $4, $6) ]}
  // Conditionals if and if-else
  | IF expr COLON suite
    {[ pos $1 $3, 
       If ([pos $1 $3, { cond = Some $2; cond_stmts = $4 }]) ]}
  | IF expr COLON suite; rest = elif_suite
    {[ pos $1 $3, 
       If ((pos $1 $3, { cond = Some $2; cond_stmts = $4 }) :: rest) ]}
  // Pattern matching
  | MATCH expr COLON NL INDENT case_suite DEDENT
    {[ pos $1 $4, 
       Match ($2, $6) ]}
  // Try statement
  | try_statement
  // Function and clas definitions
  | func_statement
  | class_statement
    {[ $1 ]}

// Simple one-line statements
small_statement: 
  // Single expression or assignment
  | expr_statement   
    { $1 }
  // Imports
  | import_statement 
    { $1 }
  // Type definitions
  | type_stmt        
    { $1 }
  // throw statement
  | throw
    { $1 }
  // pass statement
  | PASS     
    { $1, 
      Pass () }
  // loop control statements
  | BREAK    
    { $1, 
      Break () }
  | CONTINUE 
    { $1, 
      Continue () }
  // del statement
  | DEL separated_nonempty_list(COMMA, expr)
    { pos $1 (fst @@ List.last_exn $2), 
      Del $2 }
  // print statement
  | PRINT separated_list(COMMA, expr)
    { let l = Option.value_map (List.last $2) ~f:fst ~default:$1 in
      pos $1 l, 
      Print $2 }
  // assert statement
  | ASSERT expr_list
    { pos $1 (fst @@ List.last_exn $2), 
      Assert $2 }
  // return and yield statements
  | RETURN separated_list(COMMA, expr)
    { let pos, expr = match List.length $2 with
        | 0 -> $1, 
               None
        | 1 -> pos $1 (fst @@ List.last_exn $2), 
               Some (List.hd_exn $2)
        | _ -> pos $1 (fst @@ List.last_exn $2), 
               Some (pos (fst @@ List.hd_exn $2) (fst @@ List.last_exn $2), 
                     Tuple $2)
      in 
      pos, 
      Return expr }
  | YIELD separated_list(COMMA, expr)
    { let pos, expr = match List.length $2 with
        | 0 -> $1, 
               None
        | 1 -> pos $1 (fst @@ List.last_exn $2), 
               Some (List.hd_exn $2)
        | _ -> pos $1 (fst @@ List.last_exn $2), 
               Some (pos (fst @@ List.hd_exn $2) (fst @@ List.last_exn $2), 
                     Tuple $2)
      in 
      pos, 
      Yield expr }
  // global statement
  | GLOBAL separated_nonempty_list(COMMA, ID) 
    { pos $1 (fst @@ List.last_exn $2),
      Global $2 }

// Type definitions
type_stmt:
  | TYPE ID LP separated_list(COMMA, typed_param) RP 
    { pos $1 $5, 
      Type (snd $2, $4) }
// Typed argument rule where type is optional (name [ : type])
typed_param:
  | ID param_type? 
    { let last = Option.value_map $2 ~f:fst ~default:(fst $1) in
      pos (fst $1) last, 
      { name = snd $1; typ = $2 } }
// Type parameter rule (: type)
param_type:
  | COLON expr 
    { $2 }

// Expressions and assignments
// TODO: tuple assignment: https://www.python.org/dev/peps/pep-3132/ 
// TODO: a = b = c = d = ... separated_nonempty_list(EQ, expr_list) 
expr_statement: 
  // List of expressions
  // TODO: currently only one expression is allowed 
  // (so statement "expr1, expr2" is not allowed)
  | expr_list
    { assert ((List.length $1) = 1);
      let expr = List.hd_exn $1 in
      fst expr, 
      Expr expr }
  // Assignment for modifying operators
  // (+=, -=, *=, /=, %=, **=, //=)
  | expr aug_eq expr_list
    { let op = String.sub (snd $2) ~pos:0 ~len:(String.length (snd $2) - 1) in
      pos (fst $1) (fst @@ List.last_exn $3), 
      Assign ([$1], 
              [pos (fst $1) (fst @@ List.last_exn $3), 
               Binary($1, op, List.hd_exn $3)], 
              false) }
  // Assignment (a, b = x, y)
  | expr_list EQ expr_list
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $3), 
      Assign ($1, $3, false) }
  // Forced assignment (a, b := x, y)
  | expr_list ASSGN_EQ expr_list
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $3), 
      Assign ($1, $3, true) }
%inline aug_eq:
  // TODO: bit shift ops 
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ 
    { $1 }

// Suites (indented blocks of code)
suite: 
  // Same-line suite (if foo: suite)
  | separated_nonempty_list(SEMICOLON, small_statement) NL
    { $1 }
  // Indented suites
  | NL INDENT statement+ DEDENT
    { List.concat $3 }
// If/Else suites
elif_suite:
  // elif foo:
  | ELIF expr COLON suite
    {[ pos $1 $3, 
       { cond = Some $2; cond_stmts = $4 } ]}
  // else: 
  | ELSE COLON suite
    {[ pos $1 $2, 
       { cond = None; cond_stmts = $3 } ]}
  | ELIF expr COLON suite; rest = elif_suite
    { (pos $1 $3, { cond = Some $2; cond_stmts = $4 }) :: rest }
// Pattern case suites
case_suite:
  // default: 
  | DEFAULT COLON suite
    {[ pos $1 $2, 
       { pattern = WildcardPattern None; case_stmts = $3 } ]}
  // case ...:
  | case 
    {[ $1 ]}
  | case; rest = case_suite
    { $1 :: rest }
// Specific pattern suites
case:
  // case pattern
  | CASE separated_nonempty_list(OR, case_type) COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $3, 
      { pattern; case_stmts = $4 } }
  // guarded: case pattern if foo
  | CASE separated_nonempty_list(OR, case_type) IF bool_expr COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $5, 
      { pattern = GuardedPattern (pattern, $4); case_stmts = $6 } }
  // bounded: case pattern as id:
  | CASE separated_nonempty_list(OR, case_type) AS ID COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $5, 
      { pattern = BoundPattern (snd $4, pattern); case_stmts = $6 } }
// Pattern rules
case_type:
  | ELLIPSIS 
    { StarPattern }
  | ID       
    { WildcardPattern (Some (snd $1)) }
  | INT      
    { IntPattern (snd $1) }
  | bool     
    { BoolPattern (snd $1) }
  | STRING   
    { StrPattern (snd $1) }
  | SEQ      
    { SeqPattern (snd $1) }
  // Tuples & lists
  | LP separated_nonempty_list(COMMA, case_type) RP
    { TuplePattern ($2) }
  | LS separated_nonempty_list(COMMA, case_type) RS
    { ListPattern ($2) }
  // Ranges
  | INT ELLIPSIS INT
    { RangePattern(snd $1, snd $3) }

// Import statments
import_statement:
  // from x import *
  | FROM ID IMPORT MUL
    { pos $1 (fst $4),
      Import [{ from = $2; what = Some([fst $4, "*"]); 
                import_as = None; stdlib = false }] }
  // from x import y, z
  | FROM ID IMPORT separated_list(COMMA, ID)
    { pos $1 (fst @@ List.last_exn $4),
      Import [{ from = $2; what = Some($4); 
                import_as = None; stdlib = false }] }
  // import x, y
  | IMPORT separated_list(COMMA, import_term)
    { pos $1 (fst @@ List.last_exn $2), 
      Import (List.map $2 ~f:(fun (_, (from, import_as)) ->
        { from; what = None; import_as; stdlib = false })) }
  // import!
  | IMPORT_CONTEXT ID
    { pos $1 (fst $2), 
      Import [{ from = $2; what = None; 
                import_as = None; stdlib = true }] }
// Import terms (foo, foo as bar)
import_term:
  | ID
    { fst $1, 
      ($1, None) }
  | ID AS ID
    { pos (fst $1) (fst $3),
      ($1, Some (snd $3)) } 
// Dotted identifiers (foo, foo.bar)
dot_term:
  | ID
    { fst $1, 
      Id (snd $1) }
  | dot_term DOT ID
    { pos (fst $1) (fst $3), 
      Dot ($1, snd $3) }

// Try/catch statements
try_statement:
  | TRY COLON suite catch+ finally?
    { pos $1 $2, 
      Try ($3, $4, $5) }
// Catch rules
catch:
  /* TODO: except (RuntimeError, TypeError, NameError) */
  // any: except
  | EXCEPT COLON suite
    { pos $1 $2, 
      { exc = None; var = None; stmts = $3 } }
  // specific: except foo
  | EXCEPT ID COLON suite
    { pos $1 $3, 
      { exc = Some (snd $2); var = None; stmts = $4 } }
  // named: except foo as bar
  | EXCEPT ID AS ID COLON suite
    { pos $1 $5, 
      { exc = Some (snd $2); var = Some (snd $4); stmts = $6 } }
// Finally rule
finally:
  | FINALLY COLON suite
    { $3 }
// Throw statement
throw:
  | THROW expr
    { $1,
      Throw $2 }


/******************************************************************************
 ************                      UNITS                     ******************
 ******************************************************************************/

// Function statement
func_statement:
  | func { $1 }
  | decorator+ func
    { noimp "decorator"(* DecoratedFunction ($1, $2) *) }

// Function definition
func:
  // Seq function (def foo [ [type+] ] (param+) [ -> return ])
  | DEF; name = ID;
    intypes = generic_list?;
    LP params = separated_list(COMMA, func_param); RP
    typ = func_ret_type?;
    COLON;
    s = suite
    { let intypes = Option.value intypes ~default:[] in
      pos $1 $8, 
      Generic (Function 
        ((fst name, { name = snd name; typ }), intypes, params, s)) }
  // Extern function (extern lang [ (dylib) ] foo (param+) -> return)
  | EXTERN; lang = ID; dylib = dylib_spec?; name = ID;
    LP params = separated_list(COMMA, typed_param); RP
    typ = func_ret_type?; NL
    { let typ = match typ with
        | Some typ -> typ
        | None -> $7, Id("void")
      in
      pos $1 (fst typ), 
      Extern (snd lang, dylib, 
        (fst name, { name = snd name; typ = Some(typ) }), params) }
// Generic specifiers
generic_list:
  | LS; separated_nonempty_list(COMMA, generic); RS 
    { $2 }
// Parameter rule (a, a: type, a = b)
func_param:
  | typed_param 
    { $1 }
  | ID EQ expr
    { noimp "NamedArg"(*NamedArg ($1, $3)*) }
// Return type rule (-> type)
func_ret_type:
  | OF; expr 
    { $2 }
// dylib specification
dylib_spec:
  | LP STRING RP 
    { snd $2 }


// Class statement
class_statement:
  | cls
  | extend 
    { $1 }

// Classes
cls:
  // class name [ [type+] ] (param+)
  | CLASS ; n = ID;
    intypes = generic_list?
    LP; mems = separated_list(COMMA, typed_param) RP;
    COLON NL; 
    INDENT fns = class_member+ DEDENT
    { let intypes = Option.value intypes ~default:[] in
      pos $1 $7, 
      Generic (Class (snd n, intypes, mems, List.filter_opt fns)) }
// Class extensions (extend name)
extend:
  | EXTEND ; n = ID; COLON NL; 
    INDENT fns = class_member+ DEDENT
    { pos $1 $3, 
      Extend (snd n, List.filter_opt fns) }
// Class suite members
class_member:
  // Empty statements
  | PASS NL { None }
  // TODO later: | class_statement 
  // Functions
  | func_statement 
    { Some (fst $1, match snd $1 with Generic c -> c | _ -> assert false) }


// Decorators 
decorator:
  | AT dot_term NL
    { noimp "decorator" (* Decorator ($2, []) *) }
  | AT dot_term LP separated_list(COMMA, expr) RP NL
    { noimp "decorator" (* Decorator ($2, $4) *) }

