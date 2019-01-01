(******************************************************************************
 *
 * Seq OCaml 
 * err.ml: Error exceptions and helpers
 *
 * Author: inumanag
 *
 ******************************************************************************)

(** Error kind description *)
type t =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string

(** Unified exception that groups all other exceptions based on their source *)
exception CompilerError of t * Ast.Pos.t list
(** LLVM/C++ exception *)
exception SeqCError of string * Ast.Pos.t
(** AST-parsing exception  *)
exception SeqCamlError of string * Ast.Pos.t list
(** Lexing exception *)
exception SyntaxError of string * Ast.Pos.t


(** [serr ~pos format_string ...] throws an AST-parsing exception 
    with message formatted via sprintf-style [format_string]
    that indicates file position [pos] as a culprit. *)
let serr ?(pos=Ast.Pos.dummy) fmt = 
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [pos]))) fmt

(** Helper to parse string exception messages passed from C++ library to 
    OCaml and to extract [Ast.Pos] information from them. 
    Currently done in a very primitive way by using '\b' as field separator.

    TODO: pass and parse Sexp-style strings *)
let split_error msg = 
  let open Core in
  let l = Array.of_list @@ String.split ~on:'\b' msg in
  assert ((Array.length l) = 5);
  let msg = l.(0) in
  let file = l.(1) in
  let line = Int.of_string l.(2) in
  let col = Int.of_string l.(3) in
  let len = Int.of_string l.(4) in 
  raise @@ SeqCError (msg, Ast.Pos.{ file; line; col; len })

let print_error ?file typ pos_lst =
  let open Core in
  let asp = ANSITerminal.sprintf in

  let kind, msg = match typ with
    | Lexer s -> "lexer", s
    | Parser -> "parser", "Parsing error"
    | Descent s -> "descent", s
    | Compiler s -> "compiler", s 
  in
  let file_line file_name line =
    if String.length file_name > 0 && file_name.[0] <> '<' then
      try
        let lines = In_channel.read_lines file_name in 
        List.nth lines (line - 1)
      with _ -> 
        None
    else if (file_name = "<jit>") && (is_some file) then 
      try 
        let lines = String.split ~on:'\n' (Option.value_exn file) in
        List.nth lines (line - 1)
      with _ -> 
        None
    else 
      None
  in
  let style = ANSITerminal.[Bold; red] in
  eprintf "%s%!" @@ asp style "[ERROR] %s error: %s\n" kind msg;
  List.iteri pos_lst ~f:(fun i pos ->
    let Ast.Pos.{ file; line; col; len } = pos in
    match file_line file line with
    | Some file_line  ->
      let pre = if i = 0 then "" else "then in\n        " in 
      eprintf "%s%!" @@ asp style "        %s%s: %d,%d\n" 
        pre file line col;
      eprintf "%s%!" @@ asp style "   %3d: %s" 
        line (String.prefix file_line col);
      eprintf "%s%!" @@ asp 
        ANSITerminal.[Bold; white; on_red] "%s" 
        (String.sub file_line ~pos:col ~len);
      eprintf "%s%!" @@ asp style "%s" 
        (String.drop_prefix file_line (col + len));
      eprintf "%s%!" @@ asp [] "\n"
    | None -> ())
