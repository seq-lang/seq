(* *****************************************************************************
 * Seqaml.Parser: Menhir grammar description of Seq language
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

%{
  open Core
  open Ast.Expr
  open Ast.Stmt
  open Err

  let noimp s =
    ierr "cannot parse %s yet (grammar)" s

  let assign_cnt = ref 0

  let new_assign () =
    incr assign_cnt;
    sprintf "$A%d" @@ pred !assign_cnt

  (* Calculates the total span of a region
     bounded by [st] and [ed] *)
  let pos st ed =
    Ast.Ann.
      { st with pos = { st.pos with len = (ed.pos.col + ed.pos.len) - st.pos.col } }

  (* Converts list of expressions into the pipeline AST node *)
  let flat_pipe x =
    match x with
    | _, []  -> ierr "empty pipeline expression (grammar)"
    | _, [h] -> snd h
    | pos, l -> pos, Pipe l

  (* Converts list of conditionals into the AND AST node
     (used for chained conditionals such as
      0 < x < y < 10 that becomes (0 < x) AND (x < y) AND (y < 10)) *)
  type cond_t =
    | Cond of Ast.Expr.t
    | CondBinary of (Ast.Expr.t Ast.Ann.ann * string * cond_t Ast.Ann.ann)
  let rec flat_cond x =
    let expr = match snd x with
      | CondBinary (lhs, op, (_, CondBinary (next_lhs, _, _) as rhs)) ->
        Binary (
          (fst lhs, Binary (lhs, op, next_lhs)),
          "&&",
          flat_cond rhs)
      | CondBinary (lhs, op, (pos, Cond (rhs))) ->
        Binary (lhs, op, (pos, rhs))
      | Cond n ->
        n
    in
    fst x, expr
  let rec flatten_dot ~sep = function
    | pos, Id s -> s
    | pos, Dot (d, s) ->
      sprintf "%s%s%s" (flatten_dot ~sep d) sep s
    | _ -> ierr "invalid import construct (grammar)"
%}

/* constants */
%token <Ast.Ann.t * string> INT
%token <Ast.Ann.t * float> FLOAT
%token <Ast.Ann.t * (string * string)> INT_S
%token <Ast.Ann.t * (float * string)> FLOAT_S
%token <Ast.Ann.t * string> STRING ID INTERNAL
%token <Ast.Ann.t * string> SEQ KMER

/* blocks */
%token <Ast.Ann.t> INDENT
%token <Ast.Ann.t> DEDENT
%token <Ast.Ann.t> EOF
%token <Ast.Ann.t> NL        // \n
%token <Ast.Ann.t> DOT       // .
%token <Ast.Ann.t> COLON     // :
%token <Ast.Ann.t> SEMICOLON // ;
%token <Ast.Ann.t> COMMA     // ,
%token <Ast.Ann.t> OF        // ->

/* parentheses */
%token <Ast.Ann.t> LP RP     // ( ) parentheses
%token <Ast.Ann.t> LS RS     // [ ] squares
%token <Ast.Ann.t> LB RB     // { } braces

/* keywords */
%token <Ast.Ann.t> FOR WHILE CONTINUE BREAK           // loops
%token <Ast.Ann.t> IF ELSE ELIF MATCH CASE AS DEFAULT // conditionals
%token <Ast.Ann.t> DEF RETURN YIELD EXTERN LAMBDA     // functions
%token <Ast.Ann.t> TYPE CLASS TYPEOF EXTEND PTR       // types
%token <Ast.Ann.t> IMPORT FROM GLOBAL IMPORT_CONTEXT  // variables
%token <Ast.Ann.t> PRINT PASS ASSERT DEL              // keywords
%token <Ast.Ann.t> TRUE FALSE NONE                    // booleans
%token <Ast.Ann.t> TRY EXCEPT FINALLY THROW WITH      // exceptions
%token <Ast.Ann.t> PREFETCH                           // prefetch

/* operators */
%token<Ast.Ann.t * string> EQ ASSGN_EQ ELLIPSIS // =, :=, ...
%token<Ast.Ann.t * string> ADD SUB MUL DIV // +, -, *, /
%token<Ast.Ann.t * string> FDIV POW MOD AT // //, **, %, @
%token<Ast.Ann.t * string> PLUSEQ MINEQ MULEQ DIVEQ  // +=, -=, *=, /=
%token<Ast.Ann.t * string> FDIVEQ POWEQ MODEQ // //=, **=, %=,
%token<Ast.Ann.t * string> AND OR NOT // and, or, not
%token<Ast.Ann.t * string> IS ISNOT IN NOTIN // is, is not, in, not in
%token<Ast.Ann.t * string> EEQ NEQ LESS LEQ GREAT GEQ // ==, !=, <, <=, >, >=
%token<Ast.Ann.t * string> PIPE PPIPE SPIPE // |> ||> >|
%token<Ast.Ann.t * string> B_AND B_OR B_XOR B_NOT // &, |, ^, ~
%token<Ast.Ann.t * string> B_LSH B_RSH // <<, >>
%token<Ast.Ann.t * string> LSHEQ RSHEQ ANDEQ OREQ XOREQ // <<=, >>= &= |= ^=

/* operator precedence */
%left B_OR
%left B_XOR
%left B_AND
%left B_LSH B_RSH
%left ADD SUB
%left MUL DIV FDIV MOD
%left POW AT

/* entry rule for module */
%start <Ast.Stmt.t Ast.Ann.ann list> program
%%

%public separated_nonempty_trailing_list(separator, X):
  | x = X separator
    { [ x ] }
  | x = X; separator; xs = separated_nonempty_trailing_list(separator, X)
    { x :: xs }

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
    { List.concat $1 }

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
  | INT_S      { fst $1, IntS (snd $1) }
  | FLOAT_S    { fst $1, FloatS (snd $1) }
  | STRING+
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $1), 
      String (String.concat @@ List.map $1 ~f:snd) }
  | SEQ+
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $1), 
      Seq (String.concat @@ List.map $1 ~f:snd) }
  | KMER       { fst $1, Kmer (snd $1) }
  | bool       { fst $1, Bool (snd $1) }
  | tuple      { fst $1, Tuple (snd $1) }
  | lists      { fst $1, List (snd $1) }
  | dict       { fst $1, Dict (snd $1) }
  | set        { fst $1, Set (snd $1) }
  | LP expr RP { $2 }
  | tuple_gen  { $1 }
  | dict_gen   { $1 }
  | list_gen   { $1 }
  | set_gen    { $1 }
  | MUL ID     { pos (fst $1) (fst $2), Unpack (snd $2) }

// Types
bool:
  | TRUE  { $1, true  }
  | FALSE { $1, false }
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
  | LB expr_list RB
    { pos $1 $3,
      $2 }
dict: // Dictionaries: {1: 2, 3: 4}
  | LB RB
    { pos $1 $2, [] }
  | LB separated_nonempty_list(COMMA, dictitem) RB
  | LB separated_nonempty_trailing_list(COMMA, dictitem) RB
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
    { Ast.Expr.
        { var = List.map $2 ~f:snd;
          gen = flat_pipe $4;
          cond = $5; next = $6 } }
comprehension_if:
  | IF pipe_expr
    { let exp = flat_pipe $2 in
      pos $1 (fst exp),
      snd exp }

/******************************************************************************
 ************                   EXPRESSIONS                  ******************
 ******************************************************************************/

// General expression
expr:
  | pipe_expr // Pipes and other expressions
    { flat_pipe $1 }
  | ifc = pipe_expr; IF cnd = pipe_expr; ELSE elc = expr // Inline ifs
    { pos (fst ifc) (fst elc),
      IfExpr (flat_pipe cnd, flat_pipe ifc, elc) }
  | TYPEOF LP expr RP // TypeOf call
    { pos $1 $4,
      TypeOf $3 }
  | PTR LP expr RP // Ptr call
    { pos $1 $4,
      Ptr $3 }
  | LAMBDA separated_list(COMMA, ID) COLON expr // Lambdas
    { pos $1 (fst $4),
      Lambda (List.map ~f:snd $2, $4) }
expr_list:
  | separated_nonempty_list(COMMA, expr)
  | separated_nonempty_trailing_list(COMMA, expr)
    { $1 }

// The following rules are defined in the order of operator precedence:
//   pipes -> booleans -> conditionals -> arithmetics

// Pipes (|>)
pipe_expr:
  | o = bool_expr
    { fst o, ["", o] }
  | l = bool_expr; p = PIPE; r = pipe_expr
  | l = bool_expr; p = PPIPE; r = pipe_expr
  | l = bool_expr; p = SPIPE; r = pipe_expr
    { pos (fst l) (fst r),
      (snd p, l) :: (snd r) }

// Bool expressions
// (binary: and, or)
bool_expr:
  | bool_and_expr
    { $1 }
  | bool_and_expr OR bool_expr
    { pos (fst $1) (fst $3),
      Binary ($1, snd $2, $3) }
bool_and_expr:
  | cond_expr
    { flat_cond $1 }
  | cond_expr AND bool_and_expr
    { let cond = flat_cond $1 in
      pos (fst $1) (fst $3),
      Binary (cond, snd $2, $3) }

// Conditional operators
// (unary: not; binary: <, <=, >, >=, ==, !=, is, is not, in, not in)
cond_expr:
  | arith_expr
    { fst $1, Cond (snd $1) }
  // Unary condition
  | NOT cond_expr
    { pos (fst $1) (fst $2),
      Cond (Unary ("!", flat_cond $2)) }
  // Binary condition
  | arith_expr cond_op cond_expr
    { pos (fst $1) (fst $3),
      CondBinary ($1, snd $2, $3) }
%inline cond_op:
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ | IS | ISNOT | IN | NOTIN
  { $1 }

// Arithmetic expression
// (unary: +, -, ~; binary: +, -, *, /, //, %, **, &, |, ^, >>, <<)
arith_expr:
  | arith_term
    { $1 }
  // Unary operator
  | SUB+ arith_term
  | B_NOT+ arith_term
  | ADD+ arith_term
    { let cnt = List.length $1 in
      pos (fst (List.hd_exn $1)) (fst $2),
      match cnt % 2, snd (List.hd_exn $1), snd $2 with
      | 1, "~", _ -> Unary ("~", $2)
      | 1, "-", Int f -> Int ("-" ^ f)
      | 1, "-", Float f -> Float (-.f)
      | 1, "-", _ -> Unary ("-", $2)
      | _ -> snd $2
    }
  // Binary operator
  | arith_expr arith_op arith_expr
    { pos (fst $1) (fst $3),
      Binary ($1, snd $2, $3) }
%inline arith_op:
  | ADD | SUB | MUL | DIV | FDIV | MOD | POW | AT
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
      Call ($1, [{ name = None; value = (pos $2 $5, Generator ($3, $4)) }]) }
  // Index (foo[bar])
  | arith_term LS separated_nonempty_list(COMMA, index_term) RS
    { pos (fst $1) $4,
      Index ($1, $3) }
  // Access (foo.bar)
  | arith_term DOT ID
    { pos (fst $1) (fst $3),
      Dot ($1, snd $3) }
// Call arguments
call_term:
  | ELLIPSIS // For partial functions
    { { name = None; value = (fst $1, Ellipsis ()) } }
  | expr
    { { name = None; value = $1 } }
  | ID EQ expr
    { { name = Some (snd $1); value = $3 } }
  | ID EQ ELLIPSIS
    { { name = Some (snd $1); value = (fst $3, Ellipsis ()) } }
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
    { List.concat $1 }
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
       If [{ cond = Some $2; cond_stmts = $4 }] ]}
  | IF expr COLON suite; rest = elif_suite
    {[ pos $1 $3,
       If ({ cond = Some $2; cond_stmts = $4 } :: rest) ]}
  // Pattern matching
  | MATCH expr COLON NL INDENT case_suite DEDENT
    {[ pos $1 $4,
       Match ($2, $6) ]}
  // Try statement
  | try_statement
  // Function and clas definitions
  | func_statement
  | class_statement
  | decl_statement
  | with_statement
  | special_statement
    {[ $1 ]}

// Simple one-line statements
small_statement:
  // Single expression
  | expr_list
    { List.map $1 ~f:(fun expr ->
        fst expr, Expr expr) }
  // Assignment
  | assign_statement
    { $1 }
  // Imports
  | import_statement
    {[ $1 ]}
  // Type aliases
  | type_alias
    {[ $1 ]}
  // throw statement
  | throw
    {[ $1 ]}
  // pass statement
  | PASS
    {[ $1,
       Pass () ]}
  // loop control statements
  | BREAK
    {[ $1,
       Break () ]}
  | CONTINUE
    {[ $1,
       Continue () ]}
  // del statement
  | DEL separated_nonempty_list(COMMA, expr)
    { List.map $2 ~f:(fun expr ->
        fst expr, Del expr) }
  // print statement
  | PRINT
    {[ $1,
       Print ([], "\n") ]}
  | PRINT separated_nonempty_list(COMMA, expr)
    {[ pos $1 (fst @@ List.last_exn $2),
       Print ($2, "\n") ]}
  | PRINT separated_nonempty_trailing_list(COMMA, expr)
    {[ pos $1 (fst @@ List.last_exn $2),
       Print ($2, " ") ]}
  // assert statement
  | ASSERT expr_list
    { List.map $2 ~f:(fun expr ->
        fst expr, Assert expr) }
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
      [ pos,
        Return expr ]}
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
      [ pos,
        Yield expr ]}
  // global statement
  | GLOBAL separated_nonempty_list(COMMA, ID)
    { List.map $2 ~f:(fun expr ->
        fst expr, Global (snd expr)) }
  | PREFETCH separated_nonempty_list(COMMA, expr)
    {[ pos $1 (fst @@ List.last_exn $2),
       Prefetch $2 ]}


// Type definitions
type_alias:
  | TYPE ID EQ expr
    { pos $1 (fst $4),
      TypeAlias (snd $2, $4) }
// Typed argument rule where type is optional (name [ : type])
typed_param:
  | ID param_type?
    { let last = Option.value_map $2 ~f:fst ~default:(fst $1) in
      { name = snd $1; typ = $2 } }
// Type parameter rule (: type)
param_type:
  | COLON expr
    { $2 }

// Expressions and assignments
decl_statement:
  | ID COLON expr NL
    { pos (fst $1) (fst $3),
      Declare { name = snd $1; typ = Some($3) } }
assign_statement:
  // Assignment for modifying operators
  // (+=, -=, *=, /=, %=, **=, //=)
  | expr aug_eq expr
    { let op = String.sub (snd $2)
        ~pos:0 ~len:(String.length (snd $2) - 1)
      in
      [ pos (fst $1) (fst $3), AssignEq ($1, op, $3) ]}
  // Type assignment
  | ID COLON expr EQ expr
  | ID COLON expr ASSGN_EQ expr
    {[ pos (fst $1) (fst $5),
       Assign ([fst $1, Id (snd $1)], [$5],
          (if (snd $4) = ":=" then Shadow else Normal), Some $3) ]}
  // Assignment (a, b = x, y)
  | expr_list ASSGN_EQ separated_nonempty_list(ASSGN_EQ, expr_list)
  | expr_list EQ separated_nonempty_list(EQ, expr_list)
    { let shadow = if (snd $2) = ":=" then Shadow else Normal in
      let assgns = List.rev ($1 :: $3) in
      List.folding_map ~init:(List.hd_exn assgns) (List.tl_exn assgns) ~f:(fun acc lhs ->
        lhs,
        (pos (fst @@ List.hd_exn lhs) (fst @@ List.last_exn acc),
          Assign (lhs, acc, shadow, None))) }
%inline aug_eq:
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ
  | LSHEQ | RSHEQ | ANDEQ | OREQ | XOREQ
    { $1 }


// Suites (indented blocks of code)
suite:
  // Same-line suite (if foo: suite)
  | separated_nonempty_list(SEMICOLON, small_statement) NL
    { List.concat $1 }
  // Indented suites
  | NL INDENT statement+ DEDENT
    { List.concat $3 }
// If/Else suites
elif_suite:
  // elif foo:
  | ELIF expr COLON suite
    {[ { cond = Some $2; cond_stmts = $4 } ]}
  // else:
  | ELSE COLON suite
    {[ { cond = None; cond_stmts = $3 } ]}
  | ELIF expr COLON suite; rest = elif_suite
    { { cond = Some $2; cond_stmts = $4 } :: rest }
// Pattern case suites
case_suite:
  // default:
  | DEFAULT COLON suite
    {[ { pattern = WildcardPattern None; case_stmts = $3 } ]}
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
      { pattern; case_stmts = $4 } }
  // guarded: case pattern if foo
  | CASE separated_nonempty_list(OR, case_type) IF bool_expr COLON suite
    { let pattern =
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2
      in
      { pattern = GuardedPattern (pattern, $4); case_stmts = $6 } }
  // bounded: case pattern as id:
  | CASE separated_nonempty_list(OR, case_type) AS ID COLON suite
    { let pattern =
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2
      in
      { pattern = BoundPattern (snd $4, pattern); case_stmts = $6 } }
// Pattern rules
case_type:
  | ELLIPSIS
    { StarPattern }
  | ID
    { WildcardPattern (Some (snd $1)) }
  | INT
    { IntPattern (Int64.of_string @@ snd $1) }
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
    { RangePattern(Int64.of_string @@ snd $1, Int64.of_string @@ snd $3) }

// Import statments
import_statement:
  // from x import *
  | FROM dot_term IMPORT MUL
    { let from = flatten_dot ~sep:"/" $2 in
      pos $1 (fst $4),
      Import [{ from; what = Some ["*", None]; import_as = None }] }
  // from x import y, z
  | FROM dot_term IMPORT separated_list(COMMA, import_term)
    { let from = flatten_dot ~sep:"/" $2 in
      let what = List.map $4 ~f:(fun (_, (what, ias)) ->
        flatten_dot ~sep:"/" what, ias)
      in
      pos $1 (fst @@ List.last_exn $4),
      Import [{ from; what = Some what; import_as = None }] }
  // import x, y
  | IMPORT separated_list(COMMA, import_term)
    { pos $1 (fst @@ List.last_exn $2),
      Import (List.map $2 ~f:(fun (_, (from, import_as)) ->
        let from = flatten_dot ~sep:"/" from in
        { from; what = None; import_as })) }
  // import!
  | IMPORT_CONTEXT ID
    { pos $1 (fst $2),
      ImportPaste (snd $2) }
// Import terms (foo, foo as bar)
import_term:
  | dot_term
    { fst $1,
      ($1, None) }
  | dot_term AS ID
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
  | TRY COLON suite catch* finally?
    { pos $1 $2,
      Try ($3, $4, $5) }
// Catch rules
catch:
  /* TODO: except (RuntimeError, TypeError, NameError) */
  // any: except
  | EXCEPT COLON suite
    { { exc = None; var = None; stmts = $3 } }
  // specific: except foo
  | EXCEPT expr COLON suite
    { { exc = Some $2; var = None; stmts = $4 } }
  // named: except foo as bar
  | EXCEPT expr AS ID COLON suite
    { { exc = Some $2; var = Some (snd $4); stmts = $6 } }
// Finally rule
finally:
  | FINALLY COLON suite
    { $3 }
// Throw statement
throw:
  | THROW expr
    { $1,
      Throw $2 }

with_statement:
  | WITH separated_nonempty_list(COMMA, with_clause) COLON suite
    {
      let rec traverse (expr, var) lst =
        let var = Option.value var ~default:(fst expr, Id (new_assign ())) in
        let s1 = fst expr, Assign ([var], [expr], Shadow, None) in
        let s2 = fst expr, Expr (
          fst expr, Call ((fst expr, Dot (var, "__enter__")), []))
        in
        let within = match lst with
          | [] -> $4
          | hd :: tl -> [traverse hd tl]
        in
        let s3 = fst expr, Try (within, [], Some [
          fst expr, Expr (fst expr, Call ((fst expr, Dot(var, "__exit__")), []))])
        in
        fst expr, If [{ cond = Some (fst expr, Bool true); cond_stmts = [s1; s2; s3] }]
      in
      traverse (List.hd_exn $2) (List.tl_exn $2)
    }
with_clause:
  | expr
    { $1, None }
  | expr AS ID
    { $1, Some (fst $3, Id (snd $3)) }

/******************************************************************************
 ************                      UNITS                     ******************
 ******************************************************************************/

// Function statement
func_statement:
  | func { $1 }
  | decorator+ func
    {
      let fn = match snd $2 with
        | Function f -> f
        | _ -> ierr "decorator parsing failure (grammar)"
      in
      fst $2,
      Function { fn with fn_attrs = List.map ~f:snd $1 }
    }

// Function definition
func:
  // Seq function (def foo [ [type+] ] (param+) [ -> return ])
  | DEF; name = ID;
    intypes = generic_list?;
    LP fn_args = separated_list(COMMA, func_param); RP
    typ = func_ret_type?;
    COLON;
    s = suite
    { let fn_generics = Option.value intypes ~default:[] in
      pos $1 $8,
      Function
        { fn_name = { name = snd name; typ };
          fn_generics;
          fn_args;
          fn_stmts = s;
          fn_attrs = [] } }
  // Extern function (extern lang [ (dylib) ] foo (param+) -> return)
  | EXTERN; dylib = dylib_spec?; name = ID;
    LP params = separated_list(COMMA, extern_param); RP
    typ = func_ret_type?; NL
    { let typ = match typ with
        | Some typ -> typ
        | None -> $6, Id("void")
      in
      pos $1 (fst typ),
      Extern ("c", dylib, snd name, { name = snd name; typ = Some typ }, params) }
  | EXTERN; dylib = dylib_spec?; name = ID; AS alt_name = ID
    LP params = separated_list(COMMA, extern_param); RP
    typ = func_ret_type?; NL
    { let typ = match typ with
        | Some typ -> typ
        | None -> $6, Id("void")
      in
      pos $1 (fst typ),
      Extern ("c", dylib, snd name, { name = snd alt_name; typ = Some typ }, params) }

// Extern paramerers
extern_param:
  | expr
    { { name = ""; typ = Some $1 } }
  | ID param_type
    { { name = snd $1; typ = Some $2 } }
// Generic specifiers
generic_list:
  | LS; separated_nonempty_list(COMMA, ID); RS
    { List.map ~f:snd $2 }
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
  | typ
    { $1 }

// Classes
cls:
  // class name [ [type+] ] (param+)
  | CLASS ID generics = generic_list? COLON NL
    INDENT members = dataclass_member+ DEDENT
    { let args = List.filter_map members ~f:(function
        | Some (_, Declare d) -> Some d
        | _ -> None)
      in
      let args = [] in
      let members = List.filter_map members ~f:Fn.id in
      (*function
        | Some (_, Declare _) -> None
        | p -> p)
      in *)
      let args = if (List.length args) <> 0 then Some args else None in
      let generics = Option.value generics ~default:[] in
      pos $1 $5,
      Class
        { class_name = snd $2; generics; args; members } }
dataclass_member:
  | class_member { $1 }
  | decl_statement { Some $1 }


// Type definitions
typ:
  | type_head NL
    { pos (fst $1) $2,
      Type (snd $1) }
  | type_head COLON NL
    INDENT members = class_member+ DEDENT
    { pos (fst $1) $2,
      Type { (snd $1) with members = List.filter_opt members } }
type_head:
  | TYPE ID LP separated_list(COMMA, typed_param) RP
    { pos $1 $5,
      { class_name = snd $2;
        generics = [];
        args = Some $4;
        members = [] } }

// Class extensions (extend name)
extend:
  | EXTEND ; n = expr; COLON NL;
    INDENT fns = class_member+ DEDENT
    { pos $1 $3,
      Extend (n, List.filter_opt fns) }
// Class suite members
class_member:
  // Empty statements
  | PASS NL { None }
  // Functions
  | func_statement
  | class_statement
    { Some $1 }


// Decorators
decorator:
  | AT dot_term NL
    { match $2 with
      | pos, Id s -> pos, s
      | _ -> noimp "decorator dot" }
  | AT dot_term LP separated_list(COMMA, expr) RP NL
    { noimp "decorator" (* Decorator ($2, $4) *) }

special_statement:
  | INTERNAL expr STRING NL
    { match snd $1 with
      | "%typ" | "%err" ->
        pos (fst $1) $4,
        Special (snd $1, [ fst $2, Expr $2 ], [snd $3])
      | _ -> noimp "invalid special" }
  | INTERNAL ID COLON suite
    { match snd $1 with
      | "%test" ->
        pos (fst $1) $3,
        Special (snd $1, $4, [snd $2])
      | _ -> noimp "invalid special" }

