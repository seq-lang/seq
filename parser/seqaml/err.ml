(* *****************************************************************************
 * Seqaml.Error: Exceptions, errors, warnings and other logging helpers
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** Error type variant. *)
type t =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string
  | Internal of string

(** Default exception that includes the source of the error and the location where it occurred. *)
exception CompilerError of t * Ast.Ann.t list

(** LLVM/C++ exception. *)
exception SeqCError of string * Ast.Ann.t

(** Seqaml exception. *)
exception SeqCamlError of string * Ast.Ann.t list

(** Lexer exception. *)
exception SyntaxError of string * Ast.Ann.t

(** Internal exception that should never reach an user.
    Indicates either a bug or non-implement functionality. *)
exception InternalError of string * Ast.Ann.t

(** [serr ~ann format_string ...] raises an Seqaml error at file location [pos]
    with message formatted via sprintf-style [format_string]. *)
let serr ?(ann = Ast.Ann.default) fmt =
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [ ann ]))) fmt

(** [serr ~ann format_string ...] raises an Seqaml error at file location [pos]
    with message formatted via sprintf-style [format_string]. *)
(* let terr ?(ann = Ast.Ann.default) fmt =
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [ ann ]))) fmt *)

(** [ierr ~ann format_string ...] raises an internal error at file location [pos]
    with message formatted via sprintf-style [format_string]. *)
let ierr ?(ann = Ast.Ann.default) fmt =
  Core.ksprintf (fun msg -> raise (InternalError (msg, ann))) fmt

(** Helper function that parses string exception messages raised from C++
    and extracts [Ast.Ann] information from them.
    Currently done in a very primitive way (C++ library serializes messages
    by using '\b' as a field separator).*)
let split_error msg =
  let l = Array.of_list @@ String.split ~on:'\b' msg in
  assert (Array.length l = 5);
  let msg = l.(0) in
  let file = l.(1) in
  let line = Int.of_string l.(2) in
  let col = Int.of_string l.(3) in
  let len = Int.of_string l.(4) in
  raise @@ SeqCError (msg, Ast.Ann.create ~file ~line ~col ~len ())

(** [to_string ?file ~pos_lst err] returns a nicely formatted error string
    that contains the problematic source line(s) (specified in [pos_lst]) from the source [file].
    If the [file] is not given, no source code will be returned. *)
let to_string ?file ~pos_lst err =
  let kind, msg =
    match err with
    | Lexer s -> "lexer", s
    | Parser -> "parser", "Parsing error"
    | Descent s -> "descent", s
    | Compiler s -> "compiler", s
    | Internal s -> "internal", s
  in
  let file_line file_name line =
    if String.length file_name > 0 && file_name.[0] = '\t'
    then (
      let lines = String.split ~on:'\n' @@ String.drop_prefix file_name 1 in
      List.nth lines (line - 1) (* read file *))
    else if String.length file_name > 0 && file_name.[0] <> '<'
    then (
      try
        let lines = In_channel.read_lines file_name in
        List.nth lines (line - 1)
      with
      | _ -> None)
    else if file_name = "<jit>" && is_some file
    then (
      try
        let lines = String.split ~on:'\n' (Option.value_exn file) in
        List.nth lines (line - 1)
      with
      | _ -> None)
    else None
  in
  let lines =
    List.mapi pos_lst ~f:(fun i pos ->
        let Ast.Ann.{ file; line; col; len; _ } = pos in
        match file_line file line with
        | Some file_line ->
          let col =
            if col < String.length file_line then col else String.length file_line
          in
          let len =
            if col + len < String.length file_line
            then len
            else String.length file_line - col
          in
          let pre = if i = 0 then "" else "then in\n        " in
          [ sprintf "        %s%s: %d,%d\n" pre file line col
          ; sprintf "   %3d: %s" line (String.prefix file_line col)
          ; sprintf "%s" (String.sub file_line ~pos:col ~len)
          ; sprintf "%s\n" (String.drop_prefix file_line (col + len))
          ]
        | None -> [])
  in
  String.concat ~sep:"" (sprintf "[ERROR] %s error: %s\n" kind msg :: List.concat lines)
