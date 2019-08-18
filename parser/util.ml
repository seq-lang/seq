(******************************************************************************
 *
 * Seq OCaml 
 * util.ml: Utility functions and helpers
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core

(** Pretty-prints list [l] to string via [f] *)
let ppl ?(sep=", ") ~f l = 
  String.concat ~sep (List.map ~f l)

(** Output debug information to stderr (with red color or other [style])
    with sprintf-style format string
    if [SEQ_DEBUG] enviromental variable is set. *)
let dbg ?style fmt =
  let fn, fno = match Sys.getenv "SEQ_DEBUG" with 
    | Some _ -> Caml.Printf.kfprintf, Caml.Printf.fprintf
    | None -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
  in
  fn (fun o -> fno o "\n%!") stderr fmt 

let get_from_stdlib ?(ext=".seq") res =
  let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
  let paths = String.split seqpath ~on:':' in
  List.map paths ~f:(fun dir -> sprintf "%s/%s%s" dir res ext)
   |> List.find ~f:Caml.Sys.file_exists
