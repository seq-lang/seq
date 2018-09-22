(* 786 *)

open Core

exception NotImplentedError of string

type pos_t = Lexing.position
type ident = string * pos_t

type expr =
  | Bool of bool * pos_t
  | Int of int * pos_t
  | Float of float * pos_t
  | String of ident
  | Id of ident
  (* | Extern of string * string *)
  (* | Regex of string *)
  | Seq of ident

  | Generic of ident
  | TypeOf of expr * pos_t 

  | Tuple of expr list * pos_t
  (* | Generator of expr * expr *)
  (* | List of expr list  *)
  (* | ListGenerator of expr * expr *)
  (* | Set of expr list *)
  (* | SetGenerator of expr * expr *)
  (* | Dict of (expr * expr) list *)
  (* | DictGenerator of (expr * expr) * expr *)

  | IfExpr of expr * expr * expr
  (* | Lambda of vararg list * expr  *)

  | Pipe of expr list
  | Unary of ident * expr
  | Binary of expr * ident * expr
  | Index of expr * expr list
  | Slice of expr option * expr option * expr option * pos_t
  | Dot of expr * ident
  | Call of expr * expr list 

  (* | Comprehension of expr list * expr list * expr option *)
  (* | ComprehensionIf of expr *)
  | Ellipsis
and arg = 
  | Arg of ident * expr option 
  (* | NamedArg of string * expr *)

type statement =
  | Pass of pos_t 
  | Break of pos_t 
  | Continue of pos_t
  | Statements of statement list
  | Exprs of expr
  | Assign of expr * expr
  | AssignEq of expr * ident * expr
  | Print of expr list * pos_t
  | Return of expr * pos_t
  | Yield of expr * pos_t
  (* | Global of expr list  *)
  (* | Assert of expr list *)
  | Type of ident * arg list * pos_t
  | While of expr * statement list * pos_t
  | For of expr * expr * statement list * pos_t
  | If of (expr option * statement list * pos_t) list
  | Match of expr * (expr option * ident option * statement list * pos_t) list * pos_t
  | Function of arg * expr list * arg list * statement list * pos_t (* def arg [typ list] (arg list): st list *)
  | Class of ident * expr list * arg list * statement list * pos_t
  (* | DecoratedFunction of decorator list * statement *)
  (* | Import of (expr * expr option) list *)
  (* | ImportFrom of expr * ((expr * expr option) list) option *)
(* and decorator = *)
  (* | Decorator of expr * vararg list *)

type ast = 
  | Module of statement list

let sci sep lst fn =
  String.concat ~sep:sep @@ List.map ~f:fn lst
let pad l = 
  String.make (l * 2) ' ' 

let rec prn_expr prn_pos = function
  | Bool(b, pos) -> sprintf "%sBool(%b)" (prn_pos pos) b
  | Int(i, pos) -> sprintf "%sInt(%d)" (prn_pos pos) i
  | Float(f, pos) -> sprintf "%sFloat(%f)" (prn_pos pos) f
  | String(s, pos) -> sprintf "%sString(%s)" (prn_pos pos) s
  (* | Regex(s, _) -> sprintf "Regex(%s)" s *)
  | Seq(s, pos) -> sprintf "%sSeq(%s)" (prn_pos pos) s
  (* | Extern(l, v, _) -> sprintf "Extern_%s(%s)" l v *)
  | Id(i, pos) -> sprintf "%s%s" (prn_pos pos) i
  | Generic(i, pos) -> sprintf "%s%s" (prn_pos pos) i
  | Tuple(el, pos) -> sprintf "%sTuple(%s)" (prn_pos pos) @@ sci ", " el (prn_expr prn_pos)
  (* | Generator(e, ge, _) -> sprintf "Gen[%s; %s]" (prn_expr e) (prn_expr ge) *)
  (* | List(el, _) -> sprintf "List(%s)" @@ sci ", " el prn_expr *)
  (* | ListGenerator(e, ge, _) -> sprintf "ListGen[%s; %s]" (prn_expr e) (prn_expr ge) *)
  (* | Set(el, _) -> sprintf "Set(%s)" @@ sci ", " el prn_expr *)
  (* | SetGenerator(e, ge, _) -> sprintf "SetGen[%s; %s]" (prn_expr e) (prn_expr ge) *)
  (* | Dict(el, _) -> sprintf "Dict(%s)" @@ sci ", " el (fun (x, y) -> sprintf "%s:%s" (prn_expr x) (prn_expr y)) *)
  (* | DictGenerator((k, v), ge, _) -> sprintf "DictGen[%s:%s; %s]" (prn_expr k) (prn_expr v) (prn_expr ge) *)
  | IfExpr(c, i, e) -> sprintf "If(%s; %s; %s)" (prn_expr prn_pos c) (prn_expr prn_pos i) (prn_expr prn_pos e)
  (* | Lambda(v, e, _) -> sprintf "Lambda[%s; %s]" (sci ", " v prn_va) (prn_expr prn_pos e)  *)
  | Pipe(el) -> sprintf "Pipe(%s)" @@ sci ", " el (prn_expr prn_pos)
  | Binary(e, (o, pos), ee) -> sprintf "%s%s(%s; %s)" (prn_pos pos) o (prn_expr prn_pos e) (prn_expr prn_pos ee)
  | Unary((o, pos), e) -> sprintf "%s%s(%s)" (prn_pos pos) o @@ prn_expr prn_pos e
  | Index(i, el) -> sprintf "Index(%s; %s)" (prn_expr prn_pos i) (sci ", " el (prn_expr prn_pos))
  | Dot(i, (e, pos)) -> sprintf "%sDot(%s; %s)" (prn_pos pos) (prn_expr prn_pos i) e
  | Call(i, cl) -> sprintf "Call(%s; %s)" (prn_expr prn_pos i) (sci ", " cl (prn_expr prn_pos))
  | TypeOf(e, pos) -> sprintf "%sTypeOf(%s)" (prn_pos pos) (prn_expr prn_pos e)
  (* | Comprehension(fi, ei, li, _) ->  *)
    (* let cont = match li with None -> "" | Some x -> prn_expr x in *)
    (* sprintf "_For[%s; %s]%s" (sci ", " fi prn_expr) (sci ", " ei prn_expr) cont *)
  (* | ComprehensionIf(e, _) -> sprintf "_If[%s]" @@ prn_expr e *)
  | Ellipsis -> "..."
  | Slice(a, b, c, pos) ->
    let a = match a with None -> "" | Some x -> prn_expr prn_pos x in
    let b = match b with None -> "" | Some x -> prn_expr prn_pos x in
    let c = match c with None -> "" | Some x -> prn_expr prn_pos x in
    sprintf "%sSlice(%s; %s; %s)" a b c (prn_pos pos)
and prn_va prn_pos = function
  | Arg((p, pos), o) -> 
    let o = match o with 
      | None -> "any" 
      | Some x -> prn_expr prn_pos x 
    in 
    sprintf "%s%s of %s" (prn_pos pos) p o 
  (* | NamedArg(n, e, _) -> sprintf "%s = %s" n (prn_expr e) *)
let rec prn_statement level prn_pos st = 
  let s = match st with
  | Pass pos -> sprintf "%sPass"  (prn_pos pos)
  | Break pos -> sprintf "%sBreak"  (prn_pos pos)
  | Continue pos -> sprintf "%sContinue" (prn_pos pos)
  | Statements(sl) -> sprintf "Statements[\n%s]" (sci "\n" sl (prn_statement (level+1) prn_pos))
  | Exprs(el) -> sprintf "Exprs[%s]" (sci "," [el] (prn_expr prn_pos))
  | Assign(sl, el) -> sprintf "Asgn[%s; %s]" (prn_expr prn_pos sl) (prn_expr prn_pos el)
  | AssignEq(sl, (op, pos), el) -> sprintf "Asgn[%s; %s%s; %s]" (prn_expr prn_pos sl) (prn_pos pos) op (prn_expr prn_pos el)
  | Print(ell, pos) -> sprintf "%sPrint[%s]" (prn_pos pos) (sci ", " ell (prn_expr prn_pos))
  | Yield(el, pos) -> sprintf "%sYield[%s]" (prn_pos pos) (prn_expr prn_pos el)
  | Return(el, pos) -> sprintf "%sReturn[%s]" (prn_pos pos) (prn_expr prn_pos el)
  (* | Global(el, _) -> sprintf "Global[%s]" (sci ", " el (prn_expr)) *)
  (* | Assert(el, _) -> sprintf "Assert[%s]" (sci ", " el (prn_expr)) *)
  | Type((e, _), vl, pos) -> sprintf "%sType[%s; %s]" (prn_pos pos) e (sci ", " vl (prn_va prn_pos))
  | While(e, sl, pos) ->
    sprintf "%sWhile[%s;\n%s]" (prn_pos pos) (prn_expr prn_pos e) @@
      sci "\n" sl (prn_statement (level + 1) prn_pos)
  | For(sl, el, stl, pos) -> 
    sprintf "%sFor[%s; %s;\n%s]" (prn_pos pos) (prn_expr prn_pos sl) (prn_expr prn_pos el) @@
      sci "\n" stl (prn_statement (level + 1) prn_pos)
  | If(el) -> sprintf "If[\n%s]" @@ 
      sci "\n" el (fun (e, sl, pos) -> 
        let cnd = match e with | Some _e -> prn_expr prn_pos _e | None -> "_" in
        sprintf "%s%s%s -> [\n%s]" (pad (level+1)) (prn_pos pos) cnd @@
          sci "\n" sl (prn_statement (level+2) prn_pos))
  | Match(e, ml, pos) -> sprintf "%sMatch[%s;\n%s]" (prn_pos pos) (prn_expr prn_pos e) @@ 
      sci "\n" ml (fun (e, v, sl, _) -> 
        let pv = match v with | Some (e, _) -> " AS " ^ (e) | None -> "" in
        let pe = match e with | Some e -> (prn_expr prn_pos e) | None -> "DEFAULT" in
        sprintf "%s%s%s -> [\n%s]" (pad (level + 1)) pe pv @@
          sci "\n" sl (prn_statement (level + 2) prn_pos))
  (*| DecoratedFunction(dl, f) -> (sci ("\n" ^ (pad level)) dl 
        (fun d -> match d with Decorator(dd, da) -> 
          sprintf "Decorator[%s; %s]" (prn_expr dd) @@ sci ", " da prn_va)) ^ 
      (prn_statement level f) *)
  | Function(v, tl, vl, sl, pos) -> 
      sprintf "%sDef<%s>[%s; %s;\n%s]" (prn_pos pos) (sci ", " tl (prn_expr prn_pos)) (prn_va prn_pos v) (sci ", " vl (prn_va prn_pos)) @@ 
        sci "\n" sl (prn_statement (level + 1) prn_pos)
  | Class((v, _), tl, vl, sl, pos) -> 
      sprintf "%sClass<%s>[%s; %s;\n%s]" (prn_pos pos) (sci ", " tl (prn_expr prn_pos)) v (sci ", " vl (prn_va prn_pos)) @@ 
        sci "\n" sl (prn_statement (level + 1) prn_pos)
  (* | Import(el, _) -> 
    sprintf "Import[%s]" @@ sci ", " el (fun (a, b) ->
      let b = match b with None -> "" | Some x -> " as " ^ (prn_expr x) in
      (prn_expr a) ^ b)
  | ImportFrom(e, el, _) -> 
    let el = match el with None -> "all" | Some x -> sci ", " x (fun (a, b, _) ->
      let b = match b with None -> "" | Some x -> " as " ^ (prn_expr x) in
      (prn_expr a) ^ b
    ) in sprintf "Import[%s; %s]" (prn_expr e) el *)
  in (pad level) ^ s
let prn_ast prn_pos ast =
  match ast with Module sl -> sci "\n" sl (prn_statement 0 prn_pos)
