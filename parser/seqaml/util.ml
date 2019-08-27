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
  let paths = String.split seqpath ~on:':' in
  List.map paths ~f:(fun dir -> sprintf "%s/%s%s" dir res ext)
  |> List.find ~f:Caml.Sys.file_exists
