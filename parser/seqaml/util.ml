(* *****************************************************************************
 * Seqaml.Util: Utility functions and helpers
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** [ppl ~sep ~f l] concatenates a list of strings [List.map ~f l] with a separator [sep]. *)
let ppl ?(sep = ", ") ~f l = String.concat ~sep (List.map ~f l)

(** [dbg fmt ...] output a [fmt] format string to stderr if [SEQ_DEBUG] environmental variable is set. *)
let dbg fmt =
  let fn, fno =
    match Sys.getenv "SEQ_DEBUG" with
    | Some _ -> Caml.Printf.kfprintf, Caml.Printf.fprintf
    | None -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
  in
  fn (fun o -> fno o "\n%!") stderr fmt

(** [get_from_stdlib ~ext res] attempts to locate a file "res.ext" from the Seq's PATH
    (defined via environmental variable [SEQ_PATH]). *)
let get_from_stdlib ?(ext = ".seq") res =
  let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
  let path = Filename.realpath Sys.argv.(0) in
  let path = Filename.dirname @@ Filename.dirname path in
  let seqpath = sprintf "%s:%s/stdlib" seqpath path in
  (* let other_paths  *)
  let paths = String.split seqpath ~on:':' in
  List.find_map paths ~f:(fun dir ->
    (* first look for res.ext  *)
    let f = sprintf "%s/%s%s" dir res ext in
    if Caml.Sys.file_exists f then Some f else (
      let f = sprintf "%s/%s/__init__%s" dir res ext in
      if Caml.Sys.file_exists f then Some f else None
    ))

let unindent s =
  let s = String.split ~on:'\n' s |> List.filter ~f:(fun s -> String.length s > 0) in
  match s with
  | a :: l ->
    let l = String.take_while a ~f:(fun s -> s = ' ') |> String.length in
    List.map s ~f:(fun s -> String.drop_prefix s l) |> String.concat ~sep:"\n"
  | [] -> ""