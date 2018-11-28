(******************************************************************************
  786
  Menhir grammar for Seq language
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
%token <Ast.Pos.t> IMPORT FROM GLOBAL                 // variables 
%token <Ast.Pos.t> PRINT PASS ASSERT DEL              // keywords 
%token <Ast.Pos.t> TRUE FALSE NONE                    // booleans 
%token <Ast.Pos.t> TRY EXCEPT FINALLY THROW           // exceptions

/* operators */
%token<Ast.Pos.t>          EQ ASSGN_EQ ELLIPSIS
%token<Ast.Pos.t * string> ADD SUB MUL DIV FDIV POW MOD 
%token<Ast.Pos.t * string> PLUSEQ MINEQ MULEQ DIVEQ MODEQ POWEQ FDIVEQ
%token<Ast.Pos.t * string> AND OR NOT IS ISNOT IN NOTIN
%token<Ast.Pos.t * string> EEQ NEQ LESS LEQ GREAT GEQ
%token<Ast.Pos.t * string> PIPE 
%token<Ast.Pos.t * string> B_LSH B_RSH B_AND B_XOR B_NOT B_OR

/* operator precedence */
%left B_OR
%left B_XOR
%left B_AND
%left B_LSH B_RSH
%left ADD SUB
%left MUL DIV FDIV MOD
%left POW

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
 ************                      ATOMS                     ******************
 ******************************************************************************/

atom: // Basic structures: identifiers, nums/strings, tuples/list/dicts 
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
  | LP test RP { $2 }
  | dict_gen   { $1 }
  | list_gen   { $1 }
  | set_gen    { $1 }
  | REGEX      { noimp "Regex" }

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
  | LP test comprehension RP
    { noimp "Generator" (* Generator ($2, $3)  *) }
  | LP test COMMA RP
    { pos $1 $4, 
      [$2] }
  | LP test COMMA test_list RP
    { pos $1 $5, 
      $2 :: $4 }
lists: // Lists: [1, 2, 3] 
  // TODO needs trailing comma support 
  | LS RS
    { pos $1 $2, 
      [] }
  | LS test_list RS
    { pos $1 $3, 
      $2 }
list_gen:
  | LS test comprehension RS
    { pos $1 $4, 
      ListGenerator ($2, $3) }
set:
  | LB separated_nonempty_list(COMMA, test) RB
    { pos $1 $3, 
      $2 }
set_gen:
  | LB test comprehension RB
    { pos $1 $4, 
      SetGenerator ($2, $3) }
dict: // Dictionaries: {1: 2, 3: 4} 
  | LB RB
    { pos $1 $2, [] }
  | LB separated_nonempty_list(COMMA, dictitem) RB
    { pos $1 $3, 
      $2 }
dict_gen:
  | LB dictitem comprehension RB
    { pos $1 $4, 
      DictGenerator ($2, $3) }
dictitem:
  | test COLON test 
    { $1, $3 }

comprehension:
  | FOR separated_list(COMMA, ID) IN pipe_test comprehension_if? comprehension?
    { let last = match $6, $5, $4 with
        | Some (p, _), _, _
        | None, Some (p, _), _
        | None, None, (p, _) -> p
      in
      pos $1 last, 
      ExprNode.{ var = List.map $2 ~f:snd; gen = flat $4; cond = $5; next = $6 } }
comprehension_if:
  | IF pipe_test
    { let exp = flat $2 in 
      pos $1 (fst exp), 
      snd exp }

/******************************************************************************
 ************                   EXPRESSIONS                  ******************
 ******************************************************************************/

test: // General expression: 5 <= p.x[1:2:3] - 16, 5 if x else y, lambda y: y+3 
  | pipe_test
    { flat $1 }
  | ifc = pipe_test; IF cnd = pipe_test; ELSE elc = test
    { pos (fst ifc) (fst elc), 
      IfExpr (flat cnd, flat ifc, elc) }
  | TYPEOF LP test RP
    { pos $1 $4, 
      TypeOf $3 }
  | LAMBDA separated_list(COMMA, ID) COLON test
    { pos $1 (fst $4), 
      Lambda ($2, $4) }
test_list:
  | separated_nonempty_list(COMMA, test) 
    { $1 }

pipe_test: // Pipe operator: a, a |> b 
  | o = or_test 
    { fst o, [o] }
  | or_test PIPE pipe_test 
    { pos (fst $1) (fst $3), 
      $1 :: (snd $3) }
or_test: // OR operator: a, a or b 
  | and_test 
    { $1 }
  | and_test OR or_test
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
and_test: // AND operator: a, a and b 
  | not_test 
    { $1 }
  | not_test AND and_test
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
not_test: // General comparison: a, not a, a < 5 
  | expr 
    { $1 }
  | NOT not_test
    { pos (fst $1) (fst $2), 
      Unary ("!", $2) }
  | expr cond_op not_test
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
%inline cond_op:
  | LESS | LEQ | GREAT | GEQ | EEQ | NEQ | IS | ISNOT | IN | NOTIN
  { $1 }

expr_term: // Expression term: 4, a(4), a[5], a.x, p 
  | atom 
    { $1 }
  | expr_term LP; args = separated_list(COMMA, call_term); RP
    { pos (fst $1) $4, 
      Call ($1, args) }
  | expr_term LS separated_nonempty_list(COMMA, sub) RS
    // TODO: tuple index
    { pos (fst $1) $4, 
      Index ($1, $3) }
  | expr_term DOT ID
    { pos (fst $1) (fst $3), 
      Dot ($1, snd $3) }
call_term:
  | ELLIPSIS
    { $1, 
      Ellipsis () }
  | test 
    { $1 }
expr: // General arithmetic: 4, 5 + p 
  | expr_term 
    { $1 }
  | ADD expr_term
  | SUB expr_term
  | B_NOT expr_term
    { pos (fst $1) (fst $2), 
      Unary(snd $1, $2) }
  | expr bin_op expr
    { pos (fst $1) (fst $3), 
      Binary ($1, snd $2, $3) }
sub: // Subscripts: ..., a, 1:2, 1::3 
  // TODO: support args/kwargs? 
  | test 
    { $1 }
  | test? COLON test?
    { let f = Option.value_map $1 ~f:fst ~default:$2 in
      let l = Option.value_map $3 ~f:fst ~default:$2 in
      pos f l, 
      Slice ($1, $3, None) }
  | test? COLON test? COLON test?
    { let f = Option.value_map $1 ~f:fst ~default:$2 in
      let l = Option.value_map $5 ~f:fst ~default:$4 in
      pos f l, 
      Slice ($1, $3, $5) }
%inline bin_op:
  | ADD | SUB | MUL | DIV | FDIV | MOD | POW 
  | B_AND | B_OR | B_XOR | B_LSH | B_RSH
    { $1 }

/******************************************************************************
 ************                   STATEMENTS                   ******************
 ******************************************************************************/

statement: // Statements: all rules return list of statements 
  // TODO: try/except, with 
  // n.b. for/while does not support else 
  | separated_nonempty_list(SEMICOLON, small_statement) NL
    { $1 }
  | NL
    {[ $1, 
       Pass () ]}
  | WHILE test COLON suite
    {[ pos $1 $3, 
       While ($2, $4) ]}
  | FOR separated_list(COMMA, ID) IN test COLON suite
    {[ pos $1 $5, 
       For (List.map $2 ~f:snd, $4, $6) ]}
  | IF test COLON suite
    {[ pos $1 $3, 
       If ([pos $1 $3, { cond = Some $2; stmts = $4 }]) ]}
  | IF test COLON suite; rest = elif_suite
    {[ pos $1 $3, 
       If ((pos $1 $3, { cond = Some $2; stmts = $4 }) :: rest) ]}
  | MATCH test COLON NL INDENT case_suite DEDENT
    {[ pos $1 $4, 
       Match ($2, $6) ]}
  | try_statement
  | func_statement
  | class_statement
  | extend_statement
    {[ $1 ]}
small_statement: // Simple one-line statements: 5+3, print x 
  | expr_statement   
    { $1 }
  | import_statement 
    { $1 }
  | type_stmt        
    { $1 }
  | throw
    { $1 }
  | PASS     
    { $1, 
      Pass () }
  | BREAK    
    { $1, 
      Break () }
  | CONTINUE 
    { $1, 
      Continue () }
  | DEL separated_nonempty_list(COMMA, test)
    { pos $1 (fst @@ List.last_exn $2), 
      Del $2 }
  | PRINT separated_list(COMMA, test)
    { let l = Option.value_map (List.last $2) ~f:fst ~default:$1 in
      pos $1 l, 
      Print $2 }
  | ASSERT test_list
    { pos $1 (fst @@ List.last_exn $2), 
      Assert $2 }
  | RETURN separated_list(COMMA, test)
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
  | YIELD separated_list(COMMA, test)
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
  | GLOBAL separated_nonempty_list(COMMA, ID) 
    { pos $1 (fst @@ List.last_exn $2),
      Global $2 }

param_type:
  | COLON test 
    { $2 }
typed_param:
  | ID param_type? 
    { let last = Option.value_map $2 ~f:fst ~default:(fst $1) in
      pos (fst $1) last, 
      { name = snd $1; typ = $2 } }
type_stmt:
  | TYPE ID LP separated_list(COMMA, typed_param) RP 
    { pos $1 $5, 
      Type (snd $2, $4) }
expr_statement: // Expression statement: a + 3 - 5 
  | test_list
    { assert ((List.length $1) = 1);
      let expr = List.hd_exn $1 in
      fst expr, 
      Expr expr }
  | test aug_eq test_list
    // TODO: tuple assignment: https://www.python.org/dev/peps/pep-3132/ 
    // TODO: a = b = c = d = ... separated_nonempty_list(EQ, test_list) 
    { let op = String.sub (snd $2) ~pos:0 ~len:(String.length (snd $2) - 1) in
      pos (fst $1) (fst @@ List.last_exn $3), 
      Assign ([$1], 
              [pos (fst $1) (fst @@ List.last_exn $3), 
               Binary($1, op, List.hd_exn $3)], 
              false) }
  | test_list EQ test_list
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $3), 
      Assign ($1, $3, false) }
  | test_list ASSGN_EQ test_list
    { pos (fst @@ List.hd_exn $1) (fst @@ List.last_exn $3), 
      Assign ($1, $3, true) }
 %inline aug_eq:
  // TODO: bit shift ops 
  | PLUSEQ | MINEQ | MULEQ | DIVEQ | MODEQ | POWEQ | FDIVEQ 
    { $1 }

suite: // Indentation blocks
  | separated_nonempty_list(SEMICOLON, small_statement) NL
    { $1 }
  | NL INDENT statement+ DEDENT
    { List.concat $3 }
elif_suite:
  | ELIF test COLON suite
    {[ pos $1 $3, 
       { cond = Some $2; stmts = $4 } ]}
  | ELSE COLON suite
    {[ pos $1 $2, 
       { cond = None; stmts = $3 } ]}
  | ELIF test COLON suite; rest = elif_suite
    { (pos $1 $3, { cond = Some $2; stmts = $4 }) :: rest }
case_suite:
  | DEFAULT COLON suite
    {[ pos $1 $2, 
       { pattern = WildcardPattern None; stmts = $3 } ]}
  | case 
    {[ $1 ]}
  | case; rest = case_suite
    { $1 :: rest }
case:
  | CASE separated_nonempty_list(OR, case_type) COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $3, 
      { pattern; stmts = $4 } }
  | CASE separated_nonempty_list(OR, case_type) IF or_test COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $5, 
      { pattern = GuardedPattern (pattern, $4); stmts = $6 } }
  | CASE separated_nonempty_list(OR, case_type) AS ID COLON suite
    { let pattern = 
        if List.length $2 = 1 then List.hd_exn $2
        else OrPattern $2 
      in
      pos $1 $5, 
      { pattern = BoundPattern (snd $4, pattern); stmts = $6 } }
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
  | LP separated_nonempty_list(COMMA, case_type) RP
    { TuplePattern ($2) }
  | LS separated_nonempty_list(COMMA, case_type) RS
    { ListPattern ($2) }
  | INT ELLIPSIS INT
    { RangePattern(snd $1, snd $3) }
dotted_name:
  | ID
    { fst $1, 
      Id (snd $1) }
  | dotted_name DOT ID
    { pos (fst $1) (fst $3), 
      Dot ($1, snd $3) }
import_statement:
  | FROM dotted_name IMPORT MUL
    { noimp "Import" (* ImportFrom ($2, None) *) }
  | FROM dotted_name IMPORT separated_list(COMMA, import_as)
    { noimp "Import"(* ImportFrom ($2, Some $4) *) }
  | IMPORT separated_list(COMMA, import_as)
    { pos $1 (fst @@ fst @@ List.last_exn $2), 
      Import $2 }
import_as:
  | ID
    { $1, None }
  | ID AS ID
    { $1, Some (snd $3) } 

try_statement:
  | TRY COLON suite catch+ finally?
    { pos $1 $2, 
      Try ($3, $4, $5) }
catch:
  /* TODO: except (RuntimeError, TypeError, NameError) */
  | EXCEPT ID COLON suite
    { pos $1 $3, 
      { exc = snd $2; var = None; stmts = $4 } }
  | EXCEPT ID AS ID COLON suite
    { pos $1 $5, 
      { exc = snd $2; var = Some (snd $4); stmts = $6 } }
finally:
  | FINALLY COLON suite
    { $3 }
throw:
  | THROW test
    { $1,
      Throw $2 }

/******************************************************************************
 ************                    GENERICS                    ******************
 ******************************************************************************/

func_statement:
  | func { $1 }
  | decorator+ func
    { noimp "decorator"(* DecoratedFunction ($1, $2) *) }
decorator:
  | AT dotted_name NL
    { noimp "decorator" (* Decorator ($2, []) *) }
  | AT dotted_name LP separated_list(COMMA, test) RP NL
    { noimp "decorator" (* Decorator ($2, $4) *) }

generic_type_list:
  | LS; separated_nonempty_list(COMMA, generic); RS 
    { $2 }
func:
  | DEF; name = ID;
    intypes = generic_type_list?;
    LP params = separated_list(COMMA, func_param); RP
    typ = func_ret_type?;
    COLON;
    s = suite
    { let intypes = Option.value intypes ~default:[] in
      pos $1 $8, 
      Generic (Function 
        ((fst name, {name = snd name; typ}), intypes, params, s)) }
  | EXTERN; lang = ID; dylib = dylib_spec?; name = ID;
    LP params = separated_list(COMMA, typed_param); RP
    typ = func_ret_type; NL
    { pos $1 (fst typ), 
      Extern (snd lang, dylib, 
        (fst name, {name = snd name; typ = Some(typ)}), params) }
dylib_spec:
  | LP STRING RP 
    { snd $2 }
func_ret_type:
  | OF; test 
    { $2 }
func_param:
  // TODO tuple params--- are they really needed? 
  | typed_param 
    { $1 }
  | ID EQ test
    { noimp "NamedArg"(*NamedArg ($1, $3)*) }

class_statement:
  | CLASS ; n = ID;
    intypes = generic_type_list?
    LP; mems = separated_list(COMMA, typed_param) RP;
    COLON NL; 
    INDENT fns = class_member+ DEDENT
    { let intypes = Option.value intypes ~default:[] in
      pos $1 $7, 
      Generic (Class (snd n, intypes, mems, List.filter_opt fns)) }
class_member:
  | PASS NL { None }
  /* | type_stmt NL { Some $1 } */
  | class_statement 
  | func_statement 
    { Some (fst $1, match snd $1 with Generic c -> c | _ -> assert false) }
extend_statement:
  | EXTEND ; n = ID; COLON NL; 
    INDENT fns = class_member+ DEDENT
    { pos $1 $3, 
      Extend (snd n, List.filter_opt fns) }

