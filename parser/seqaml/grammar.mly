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
    Ast.Ann.{ st with len = (ed.col + ed.len) - st.col }

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
    | pos, Id s -> pos, s
    | pos, Dot (d, s) ->
      pos, sprintf "%s%s%s" (snd @@ flatten_dot ~sep d) sep s
    | _ -> ierr "invalid import construct (grammar)"
%}

/* constants */
%token <Ast.Ann.t * string> INT
%token <Ast.Ann.t * float> FLOAT
%token <Ast.Ann.t * (string * string)> INT_S
%token <Ast.Ann.t * (float * string)> FLOAT_S
%token <Ast.Ann.t * string> STRING ID FSTRING
%token <Ast.Ann.t * string * string> SEQ
%token <Ast.Ann.t * string> KMER

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
%token <Ast.Ann.t> IF ELSE ELIF MATCH CASE AS         // conditionals
%token <Ast.Ann.t> DEF RETURN YIELD LAMBDA PYDEF      // functions
%token <Ast.Ann.t> TYPE CLASS TYPEOF EXTEND PTR       // types
%token <Ast.Ann.t> IMPORT FROM GLOBAL IMPORT_CONTEXT  // variables
%token <Ast.Ann.t> PRINT PASS ASSERT DEL              // keywords
%token <Ast.Ann.t> TRUE FALSE NONE                    // booleans
%token <Ast.Ann.t> TRY EXCEPT FINALLY THROW WITH      // exceptions
%token <Ast.Ann.t> PREFETCH                           // prefetch
%token <Ast.Ann.t * string> EXTERN

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

// http://gallium.inria.fr/blog/lr-lists/
reverse_separated_nonempty_llist(separator, X):
  | x = X { [ x ] }
  | xs = reverse_separated_nonempty_llist(separator, X); separator; x = X
      { x :: xs }

%inline reverse_separated_llist(separator, X):
  | { [] }
  | xs = reverse_separated_nonempty_llist(separator, X) { xs }

%inline separated_llist(separator, X):
  | xs = reverse_separated_llist(separator, X) { List.rev xs }

%inline flexible_list(delim, X):
  | xs = separated_llist(delim, X) delim? { xs }

%inline flexible_nonempty_list(delim, X):
  | x = X { [x] }
  | x = X; delim; xs = separated_llist(delim, X) delim? { x :: xs }

%inline flexible_nonempty_list_flag(delim, X):
  | x = X { [x], false }
  | x = X; delim; xs = separated_llist(delim, X); f= delim?
    { x :: xs, Option.is_some f }


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
  | FSTRING+
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $1),
      FString (String.concat @@ List.map $1 ~f:snd) }
  | SEQ+
    { let f3 (x, _, _) = x in
      let s3 (_, x, _) = x in
      let t = s3 @@ List.hd_exn $1 in
      pos (f3 @@ List.hd_exn $1) (f3 @@ List.last_exn $1),
      Seq (t, String.concat @@ List.map $1 ~f:(fun (pos, i, j) ->
        if i <> t
        then Err.serr ~pos "cannot concatenate sequences with different types"
        else j)) }
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
    { pos $1 $2, [] }
  | LP expr COMMA RP
    { pos $1 $4, [$2] }
  | LP expr COMMA flexible_nonempty_list(COMMA, expr) RP
    { pos $1 $5, $2 :: $4 }
lists: // Lists: [1, 2, 3]
  | LS flexible_list(COMMA, expr) RS
    { pos $1 $3, $2 }
set:
  | LB flexible_nonempty_list(COMMA, expr) RB
    { pos $1 $3, $2 }
dict: // Dictionaries: {1: 2, 3: 4}
  | LB flexible_list(COMMA, dictitem) RB
    { pos $1 $3, $2 }
dictitem:
  | expr COLON expr
    { $1, $3 }

// Generators
tuple_gen:
  | LP expr comprehension RP
    { pos $1 $4, Generator ($2, $3) }
list_gen:
  | LS expr comprehension RS
    { pos $1 $4, ListGenerator ($2, $3) }
set_gen:
  | LB expr comprehension RB
    { pos $1 $4, SetGenerator ($2, $3) }
dict_gen:
  | LB dictitem comprehension RB
    { pos $1 $4, DictGenerator ($2, $3) }

// Comprehensions
comprehension:
  | FOR flexible_nonempty_list(COMMA, ID) IN pipe_expr comprehension_if? comprehension?
    { let last = match $6, $5, $4 with
        | Some (p, _), _, _
        | None, Some (p, _), _
        | None, None, (p, _) -> p
      in
      pos $1 last,
      Ast.Expr.
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
      Lambda ($2, $4) }
expr_list:
  | separated_nonempty_list(COMMA, expr)
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
  | arith_term LP; args = flexible_list(COMMA, call_term); RP
    { pos (fst $1) $4,
      Call ($1, args) }
  // Call with single generator parameter without parentheses
  // (foo(x for x in y))
  | arith_term LP; expr comprehension; RP
    { pos (fst $1) $5,
      Call ($1, [pos $2 $5,
                 { name = None; value = (pos $2 $5, Generator ($3, $4)) }]) }
  // Index (foo[bar])
  | arith_term LS index_term RS
   { pos (fst $1) $4, Index ($1, $3) }
  | arith_term LS index_term COMMA RS
    { pos (fst $1) $4, Index ($1, (fst $3, Tuple [$3])) }
  | arith_term LS index_term COMMA flexible_nonempty_list(COMMA, index_term) RS
    { pos (fst $1) $6,
      Index ($1, (pos (fst $3) (fst @@ List.last_exn $5), Tuple ($3 :: $5))) }
  // Access (foo.bar)
  | arith_term DOT ID
    { pos (fst $1) (fst $3),
      Dot ($1, snd $3) }
// Call arguments
call_term:
  | ELLIPSIS // For partial functions
    { fst $1,
      { name = None; value = (fst $1, Ellipsis ()) } }
  | expr
    { fst $1,
      { name = None; value = $1 } }
  | ID EQ expr
    { pos (fst $1) (fst $3),
      { name = Some (snd $1); value = $3 } }
  | ID EQ ELLIPSIS
    { pos (fst $1) (fst $3),
      { name = Some (snd $1); value = (fst $3, Ellipsis ()) } }
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
  | flexible_nonempty_list(SEMICOLON, small_statement) NL
    { List.concat $1 }
  // Empty statement
  | NL
    {[ $1,
       Pass () ]}
  // Loops
  | WHILE expr COLON suite
    {[ pos $1 $3,
       While ($2, $4) ]}
  | FOR flexible_nonempty_list(COMMA, ID) IN expr COLON suite
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
  // Function and clas definitions
  | func_statement
    { $1 }
  // Try statement
  | try_statement
  | class_statement
  | decl_statement
  | with_statement
  /* | special_statement */
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
  | DEL flexible_nonempty_list(COMMA, expr)
    { List.map $2 ~f:(fun expr ->
        fst expr, Del expr) }
  // print statement
  | PRINT
    { [ $1, Print ([], "\n") ]}
  | PRINT flexible_nonempty_list_flag(COMMA, expr)
    { [ $1, Print (fst $2, if snd $2 then " " else "\n") ]}
  // assert statement
  | ASSERT flexible_nonempty_list(COMMA, expr)
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
  | YIELD FROM expr
    {
      (* for i in expr: yield i *)
      let p = pos $1 (fst $3) in
      let vname = new_assign () in
      let var = p, Id (vname) in
      let expr = p, Yield (Some var) in
      [ p, For ([vname], $3, [expr]) ]
    }
  // global statement
  | GLOBAL flexible_nonempty_list(COMMA, ID)
    { List.map $2 ~f:(fun expr ->
        fst expr, Global (snd expr)) }
  | PREFETCH flexible_nonempty_list(COMMA, expr)
    {[ pos $1 (fst @@ List.last_exn $2),
       Prefetch $2 ]}

// Type definitions
type_alias:
  | TYPE ID EQ expr
    { pos $1 (fst $4),
      TypeAlias (snd $2, $4) }
// Typed argument rule where type is optional (name [ : type])
typed_param:
  | ID param_type? default_val?
    { let last = Option.value_map $2 ~f:fst ~default:(fst $1) in
      pos (fst $1) last,
      { name = snd $1; typ = $2; default = $3 } }
// Type parameter rule (: type)
param_type:
  | COLON expr
    { $2 }
default_val:
  | EQ expr
    { $2 }

// Expressions and assignments
decl_statement:
  | ID COLON expr NL
    { pos (fst $1) (fst $3),
      Generic (Declare { name = snd $1; typ = Some($3); default = None }) }
assign_statement:
  // Assignment for modifying operators
  // (+=, -=, *=, /=, %=, **=, //=)
  | expr aug_eq expr
    { let op = String.sub (snd $2)
        ~pos:0 ~len:(String.length (snd $2) - 1)
      in
      let rhs =
        pos (fst $1) (fst $3),
        Binary ($1, "inplace_" ^ op, $3)
      in
      [ fst rhs,
        Assign ($1, rhs, Update, None) ]}
  // Type assignment
  | ID COLON expr EQ expr
  | ID COLON expr ASSGN_EQ expr
    {[ pos (fst $1) (fst $5),
       Assign ((fst $1, Id (snd $1)), $5,
          (if (snd $4) = ":=" then Shadow else Normal), Some $3) ]}
  // Assignment (a, b = x, y)
  | expr_list ASSGN_EQ separated_nonempty_list(ASSGN_EQ, expr_list)
  | expr_list EQ separated_nonempty_list(EQ, expr_list)
    {
      let sides = $1 :: $3 in
      let p = pos (fst @@ List.hd_exn @@ List.hd_exn sides)
                  (fst @@ List.last_exn @@ List.last_exn sides)
      in
      let shadow = if (snd $2) = ":=" then Shadow else Normal in
      let rec parse_assign lhs rhs =
        (* wrap RHS in tuple for consistency (e.g. x, y -> (x, y)) *)
        let init_exprs, rhs =
          if List.length rhs > 1 then begin
            let var = p, Id (new_assign ()) in
            [ p, Assign (var, (p, Tuple rhs), Shadow, None) ], var
          end else if List.length lhs > 1 then begin
            let var = p, Id (new_assign ()) in
            [ p, Assign (var, List.hd_exn rhs, Shadow, None) ], var
          end else
            [], List.hd_exn rhs
        in
        (* wrap LHS in tuple as well (e.g. x, y -> (x, y)) *)
        let lhs = match lhs with
          | [_, Tuple(lhs)]
          | [_, List(lhs)] ->
            lhs
          | lhs -> lhs
        in
        let exprs = match lhs with
          | [lhs] ->
            [ p, Assign (lhs, rhs, shadow, None) ]
          | lhs ->
            let len = List.length lhs in
            let unpack_i = ref (-1) in
            let lst = List.concat @@ List.mapi lhs ~f:(fun i expr ->
              match expr with
              | _, Unpack var when !unpack_i = -1 ->
                unpack_i := i;
                let start = Some (p, Int (string_of_int i)) in
                let eend =
                  if i = len - 1 then None
                  else Some (p, Int (string_of_int @@ i +  1 - len))
                in
                let slice = Slice (start, eend, None) in
                let rhs = p, Index (rhs, (p, slice)) in
                [p, Assign ((p, Id var), rhs, shadow, None)]
              | pos, Unpack var when !unpack_i > -1 ->
                Err.serr ~pos "cannot have two tuple unpackings on LHS"
              | _ when !unpack_i = -1 ->
                (* left of unpack: a, b, *c = x <=> a = x[0]; b = x[1] *)
                let rhs = p, Index (rhs, (p, Int (string_of_int i))) in
                parse_assign [expr] [rhs]
                (*p, Assign (expr, rhs, shadow) *)
              | _ ->
                (* right of unpack: *c, b, a = x <=> a = x[-1]; b = x[-2] *)
                let rhs = p, Index (rhs, (p, Int (string_of_int @@ i - len))) in
                parse_assign [expr] [rhs])
            in
            let len, op =
              if !unpack_i > -1 then
                len - 1, ">="
              else
                len, "=="
            in
            let assert_stmt = p, Assert (p, Binary (
                (p, Call ((p, Id "len"),
                          [p, { name = None; value = rhs }])),
                op,
                (p, Int (string_of_int len))))
            in
            (*assert_stmt :: *)
            lst
        in
        init_exprs @ exprs
      in
      let sides = List.rev sides in
      List.concat @@ List.folding_map (List.tl_exn sides)
        ~init:(List.hd_exn sides) (* rhs *)
        ~f:(fun rhs lhs ->
            let result = parse_assign lhs rhs in
            rhs, result) }
%inline aug_eq:
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ
  | LSHEQ | RSHEQ | ANDEQ | OREQ | XOREQ
    { $1 }


// Suites (indented blocks of code)
suite:
  // Same-line suite (if foo: suite)
  | flexible_nonempty_list(SEMICOLON, small_statement) NL
    { List.concat $1 }
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
  // case ...:
  | case
    {[ $1 ]}
  | case; rest = case_suite
    { $1 :: rest }
// Specific pattern suites
case:
  // case pattern
  | CASE case_or COLON suite
    {
      pos $1 $3,
      { pattern = $2; case_stmts = $4 } }
  // guarded: case pattern if foo
  | CASE case_or IF bool_expr COLON suite
    { pos $1 $5,
      { pattern = GuardedPattern ($2, $4); case_stmts = $6 } }
  // bounded: case pattern as id:
  | CASE case_or AS ID COLON suite
    { pos $1 $5,
      { pattern = BoundPattern (snd $4, $2); case_stmts = $6 } }
case_int:
  | INT { Int64.of_string @@ snd $1 }
  | ADD INT { Int64.of_string @@ snd $2 }
  | SUB INT { Int64.neg (Int64.of_string @@ snd $2) }
case_or:
  | p = separated_nonempty_list(OR, case_type)
    {
      if List.length(p) = 1 then List.hd_exn p else OrPattern(p)
    }
// Pattern rules
case_type:
  | ELLIPSIS
    { StarPattern }
  | ID
    { match snd $1 with "_" -> WildcardPattern None | s -> WildcardPattern (Some s) }
  | case_int
    { IntPattern ($1) }
  | bool
    { BoolPattern (snd $1) }
  | STRING
    { StrPattern (snd $1) }
  | SEQ
    { let pos, p, s = $1 in
      if p = "s" then SeqPattern s else Err.serr ~pos "cannot match protein sequences" }
  // Tuples & lists
  | LP separated_nonempty_list(COMMA, case_or) RP
    { TuplePattern ($2) }
  | LS flexible_list(COMMA, case_or) RS
    { ListPattern ($2) }
  // Ranges
  | case_int ELLIPSIS case_int
    { RangePattern($1, $3) }

// Import statments
import_statement:
  // from x import *
  | FROM dot_term IMPORT MUL
    { let from = flatten_dot ~sep:"/" $2 in
      pos $1 (fst $4),
      Import [{ from; what = Some([fst $4, ("*", None)]);
                import_as = None }] }
  // from x import y, z
  | FROM dot_term IMPORT flexible_nonempty_list(COMMA, import_term)
    { let from = flatten_dot ~sep:"/" $2 in
      let what = List.map $4 ~f:(fun (pos, (what, ias)) ->
        pos, (snd @@ flatten_dot ~sep:"/" what, ias))
      in
      pos $1 (fst @@ List.last_exn $4),
      Import [{ from; what = Some what; import_as = None }] }
  // import x, y
  | IMPORT flexible_nonempty_list(COMMA, import_term)
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

with_statement:
  | WITH flexible_nonempty_list(COMMA, with_clause) COLON suite
    {
      let rec traverse (expr, var) lst =
        let var = Option.value var ~default:(fst expr, Id (new_assign ())) in
        let s1 = fst expr, Assign(var, expr, Shadow, None) in
        let s2 = fst expr, Expr (
          fst expr, Call((fst expr, Dot(var, "__enter__")), []))
        in
        let within = match lst with
          | [] -> $4
          | hd :: tl -> [traverse hd tl]
        in
        let s3 = fst expr, Try(within, [], Some [
          fst expr, Expr (fst expr, Call((fst expr, Dot(var, "__exit__")), []))])
        in
        fst expr, If [fst expr,
          { cond = Some (fst expr, Bool true); cond_stmts = [s1; s2; s3] }]
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
  | func { [$1] }
  | pyfunc { $1 }
  | decorator+ func
    {
      let fn = match snd $2 with
        | Generic Function f -> f
        | _ -> ierr "decorator parsing failure (grammar)"
      in
      [ fst $2,
       Generic (Function { fn with fn_attrs = $1 }) ]
    }

// Function definition
func:
  // Seq function (def foo [ [type+] ] (param+) [ -> return ])
  | DEF; name = ID;
    intypes = generic_list?;
    LP fn_args = flexible_list(COMMA, typed_param); RP
    typ = func_ret_type?;
    col = COLON;
    s = suite
    { let fn_generics = Option.value intypes ~default:[] in
      pos $1 col,
      Generic (Function
        { fn_name = { name = snd name; typ; default = None };
          fn_generics;
          fn_args;
          fn_stmts = s;
          fn_attrs = [] }) }
  | extern { $1 }
// Extern function (extern lang [ (dylib) ] foo (param+) -> return)
extern:
  | from = extern_from?; lang = EXTERN; what = flexible_nonempty_list(COMMA, extern_what); NL
    {
      pos (match from with Some (p, _) -> p | None -> fst lang) $4,
      ImportExtern ( List.map what ~f:(fun e -> { e with lang = snd lang; e_from = from }) )
    }
extern_what:
  | name = ID; LP; p = flexible_list(COMMA, extern_param); RP; typ = func_ret_type?;
    eas = extern_as?
  {
    let typ = match typ with
      | Some typ -> typ
      | None -> $4, Id "void"
    in
    { lang = ""
    ; e_from = None
    ; e_name = { name = snd name; typ = Some typ; default = None }
    ; e_args = p
    ; e_as = Option.map eas ~f:snd
    }
  }
extern_from:
  | FROM dot_term
    { pos $1 (fst $2), snd $2 }
extern_as:
  | AS ID { $2 }

// Extern paramerers
extern_param:
  | expr
    { fst $1,
      { name = ""; typ = Some $1; default = None } }
  | ID param_type
    { pos (fst $1) (fst $2),
      { name = snd $1; typ = Some $2; default = None } }
// Generic specifiers
generic_list:
  | LS; flexible_nonempty_list(COMMA, ID); RS
    { $2 }
// Return type rule (-> type)
func_ret_type:
  | OF; expr
    { $2 }


pyfunc:
  // Seq function (def foo [ [type+] ] (param+) [ -> return ])
  | PYDEF; name = ID;
    LP fn_args = flexible_list(COMMA, typed_param); RP
    typ = func_ret_type?;
    COLON;
    s = suite
    { let str = Util.ppl ~sep:"\n" s ~f:(Ast.Stmt.to_string ~pythonic:true ~indent:1) in
      let p = $7 in
      (* py.exec ("""def foo(): [ind] ... """) *)
      (* from __main__ pyimport foo () -> ret *)
      let v = p, String
        (sprintf "def %s(%s):\n%s\n"
          (snd name)
          (Util.ppl fn_args ~f:(fun (_, { name; _ }) -> name))
          str) in
      let s = p, Call (
        (p, Id "_py_exec"),
        [p, { name = None; value = v }]) in
      let typ = Option.value typ ~default:($5, Id "pyobj") in
      let s' = p, ImportExtern
        [ { lang = "py"
          ; e_name = { name = snd name; typ = Some typ; default = None }
          ; e_args = []
          ; e_as = None
          ; e_from = Some (p, Id "__main__") } ]
      in
      [ p, Expr s; s' ]
    }

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
        | Some(p, Declare d) -> Some(p, d)
        | _ -> None)
      in
      let members = List.filter_map members ~f:(function
        | Some(p, Declare d) -> None
        | p -> p)
      in
      let args = if (List.length args) <> 0 then Some(args) else None in
      let generics = Option.value generics ~default:[] in
      pos $1 $5,
      Generic (Class
        { class_name = snd $2;
          generics; args; members }) }
dataclass_member:
  | class_member { $1 }
  | decl_statement
    { Some (fst $1, match snd $1 with Generic c -> c | _ -> assert false) }


// Type definitions
typ:
  | type_head NL
    { pos (fst $1) $2,
      Generic (Type (snd $1)) }
  | type_head COLON NL
    INDENT members = class_member+ DEDENT
    { pos (fst $1) $2,
      Generic (Type { (snd $1) with members = List.filter_opt members }) }
type_head:
  | TYPE ID LP flexible_list(COMMA, typed_param) RP
    { List.iter $4 ~f:(fun (_, x) ->
      if is_some x.default then Err.serr ~pos:(pos $1 $5) "type definitions cannot have default arguments");
      pos $1 $5,
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
  | PASS NL | STRING NL { None }
  // TODO later: | class_statement
  // Functions
  | func_statement
    { match $1 with
      | [l] -> Some (fst l, match snd l with Generic c -> c | _ -> assert false)
      | l -> Err.serr ~pos:(fst @@ List.hd_exn l) "no pydefs allowed in classes" }

// Decorators
decorator:
  | AT dot_term NL
    { match $2 with
      | pos, Id s -> pos, s
      | _ -> noimp "decorator dot" }
  | AT dot_term LP flexible_list(COMMA, expr) RP NL
    { noimp "decorator" (* Decorator ($2, $4) *) }

