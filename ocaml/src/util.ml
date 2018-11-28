(* 786 *)

open Core

module A = ANSITerminal 

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

let wrap style str =
  sprintf "\027[%sm%s" 
    (List.map style ~f:style_to_string |> String.concat ~sep:";") str

let sci lst ?sep fn =
  let sep = Option.value sep ~default:", " in
  String.concat ~sep @@ List.map ~f:fn lst

let dbg ?style fmt =
  let fn, fno = match Sys.getenv "SEQ_DEBUG" with 
    | Some _ -> Caml.Printf.kfprintf, Caml.Printf.fprintf
    | None -> Caml.Printf.ikfprintf, Caml.Printf.ifprintf
  in
  let style = Option.value ~default:[A.Foreground A.Red] style in 
  let codes = List.map style ~f:style_to_string |> String.concat ~sep:";" in
  Caml.Printf.fprintf stderr "\027[%sm" codes;
  fn (fun o -> fno o "\027[0m \n%!") stderr fmt 

module Option = 
struct
  include Option
  
  let unit_map ~f ~default a = 
    match a with 
    | Some a -> f a
    | None -> default ()
end
