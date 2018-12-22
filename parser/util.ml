(******************************************************************************
 *
 * Seq OCaml 
 * util.ml: Utility functions and helpers
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core

(** Colors a string for terminal printing.
    TODO: Use Fmt library *)
let style_to_string s = 
  let open ANSITerminal in
  match s with
  | Reset -> "0"
  | Bold -> "1"
  | Underlined -> "4"
  | Blink -> "5"
  | Inverse -> "7"
  | Hidden -> "8"
  | Foreground Black -> "30"
  | Foreground Red -> "31"
  | Foreground Green -> "32"
  | Foreground Yellow -> "33"
  | Foreground Blue -> "34"
  | Foreground Magenta -> "35"
  | Foreground Cyan -> "36"
  | Foreground White -> "37"
  | Foreground Default -> "39"
  | Background Black -> "40"
  | Background Red -> "41"
  | Background Green -> "42"
  | Background Yellow -> "43"
  | Background Blue -> "44"
  | Background Magenta -> "45"
  | Background Cyan -> "46"
  | Background White -> "47"
  | Background Default -> "49"

(** Pretty-prints list [l] to string via [f] *)
let ppl ?(sep=", ") ~f l = 
  String.concat ~sep (List.map ~f l)

(** Output debug information to stderr (with red color or other [style])
    with sprintf-style format string
    if [SEQ_DEBUG] enviromental variable is set. *)
let dbg ?style fmt =
  let open ANSITerminal in
  let fn, fno = match Sys.getenv "SEQ_DEBUG" with 
    | Some _ -> Caml.Printf.kfprintf, Caml.Printf.fprintf
    | None -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
  in
  let style = Option.value ~default:[Foreground Red] style in 
  let codes = List.map style ~f:style_to_string |> String.concat ~sep:";" in
  Caml.Printf.fprintf stderr "\027[%sm" codes;
  fn (fun o -> fno o "\027[0m \n%!") stderr fmt 

