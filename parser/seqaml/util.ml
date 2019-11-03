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
    | Some s when String.length s > 0 -> Caml.Printf.kfprintf, Caml.Printf.fprintf
    | _ -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
  in
  fn (fun o -> fno o "\n%!") stderr fmt

(** [get_from_stdlib ~ext res] attempts to locate a file "res.ext" from the Seq's PATH
    (defined via environmental variable [SEQ_PATH]). *)
let get_from_stdlib ?(ext = ".seq") res =
  let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
  let paths = String.split seqpath ~on:':' in
  List.map paths ~f:(fun dir -> sprintf "%s/%s%s" dir res ext) |> List.find ~f:Caml.Sys.file_exists

let ignore2 _ _ = ()
let fst3 (f, _, _) = f
let pad l = String.make (l * 2) ' '

module List = struct
  include List

  let ignore_map ~f t = ignore (map ~f t)
end


(* ANSITerminal codes *)
module A = struct
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

  let dcol ?(force = false) ?(empty = false) ?(el = true) style fmt =
    let fn, fno =
      match force, Sys.getenv "SEQ_DEBUG" with
      | true, _ -> Caml.Printf.kfprintf, Caml.Printf.fprintf
      | false, Some s when String.length s > 0 && not empty -> Caml.Printf.kfprintf, Caml.Printf.fprintf
      | _ -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
    in
    let codes = List.map style ~f:style_to_string |> String.concat ~sep:";" in
    Caml.Printf.fprintf stderr "\027[%sm" codes;
    fn (fun o -> fno o (if el then "\027[0m \n%!" else "\027[0m %!")) stderr fmt

  let dr ?(force = false) fmt = dcol ~force ANSITerminal.[ Foreground Red ] fmt
  let dbg fmt = dr fmt
  let dg ?(force = false) ?(el = true) fmt = dcol ~el ~force ANSITerminal.[ Foreground Green ] fmt
  let db ?(force = false) ?(el = true) fmt = dcol ~el ~force ANSITerminal.[ Foreground Blue ] fmt

  let dy ?(force = false) ?(el = true) fmt =
    dcol ~el ~force ANSITerminal.[ Background Yellow; Foreground Red; Bold ] fmt
end

let bop2magic ?(prefix = "") op =
  let op =
    match op with
    | "+" -> "add"
    | "-" -> "sub"
    | "*" -> "mul"
    | "/" -> "truediv"
    | "//" -> "floordiv"
    | "**" -> "pow"
    | "%" -> "mod"
    | "@" -> "mathmul"
    | "==" -> "eq"
    | "!=" -> "ne"
    | "<" -> "lt"
    | ">" -> "gt"
    | "<=" -> "le"
    | ">=" -> "ge"
    | "&" -> "and"
    | "|" -> "or"
    | "^" -> "xor"
    | "<<" -> "lshift"
    | ">>" -> "rshift"
    | "in" -> "contains"
    (* patches for C++ transform *)
    (* | "&&" -> "and" *)
    (* | "||" -> "or" *)
    | s -> s
  in
  sprintf "__%s%s__" prefix op

and uop2magic op =
  let op =
    match op with
    | "+" -> "pos"
    | "-" -> "neg"
    | "~" -> "invert"
    | "!" | "not" -> "not"
    | s -> s
  in
  sprintf "__%s__" op

let unindent s =
  let s = String.split ~on:'\n' s |> List.filter ~f:(fun s -> String.length s > 0) in
  match s with
  | a :: l ->
    let l = String.take_while a ~f:(fun s -> s = ' ') |> String.length in
    List.map s ~f:(fun s -> String.drop_prefix s l) |> String.concat ~sep:"\n"
  | [] -> ""
