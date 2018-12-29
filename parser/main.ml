(******************************************************************************
 *
 * Seq OCaml 
 * main.ml: Entry point module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err


let jit_code (ctx: Ctx.t) cnt code = 
  let anon_fn = Llvm.Func.func (sprintf "<anon_%d>" cnt) in
  let anon_ctx = 
    { ctx with 
      base = anon_fn; 
      block = Llvm.Block.func anon_fn;
      map = Hashtbl.copy ctx.map } 
  in 
  Ctx.add_block anon_ctx;
  Parser.parse_string ~file:"<jit>" ~jit:true anon_ctx code;

  Hash_set.iter (Stack.pop_exn anon_ctx.stack) ~f:(fun key ->
    match Hashtbl.find ctx.map key with
    | Some ((v, ann) :: items) -> 
      if ann.toplevel && ann.global && (not ann.internal) then 
        Ctx.add ctx ~toplevel:true ~global:true key v;
    | _ -> ());
  Llvm.JIT.func ctx.mdl anon_fn

let jit_repl () = 
  let style = ANSITerminal.[Bold; green] in
  let banner = "====================================" in
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "%s" banner;
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "Seq JIT";
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "%s" banner;

  let jit = Llvm.JIT.init () in
  let ctx = Ctx.init "<jit>"
    ~argv:false
    jit Ctypes.null Ctypes.null
    Parser.parse_file 
  in
  let code = ref "" in
  let cnt = ref 1 in 
  try while true do 
    eprintf "%s %!" @@ ANSITerminal.sprintf style "in[%d]>" !cnt;
    begin match In_channel.(input_line_exn stdin) with
    | ";;" ->
      jit_code ctx !cnt !code;
      code := "";
    | s ->
      code := (!code ^ s ^ "\n");
    end;
    cnt := !cnt + 1;
  done with End_of_file ->
    eprintf "%s\n%!" @@ 
      ANSITerminal.sprintf style "end (%d queries processed)" !cnt

(** Entry point *)
let () =
  let _ = Callback.register "parse_c" Parser.parse_c in

  if Array.length Sys.argv < 2 then
    jit_repl ()
  else
    try
      let err_handler = fun a b -> raise (CompilerError (a, b)) in
      let m = Parser.init Sys.argv.(1) err_handler in
      match m with
      | Some m -> 
        begin try
          Llvm.Module.exec m (Array.to_list Sys.argv) false
        with SeqCError (msg, pos) -> 
          raise @@ CompilerError (Compiler msg, [pos])
        end
      | None -> raise 
        Caml.Not_found
    with CompilerError (typ, pos_lst) as err ->
      let kind, msg = match typ with
        | Lexer s -> "lexer", s
        | Parser -> "parser", "Parsing error"
        | Descent s -> "descent", s
        | Compiler s -> "compiler", s 
      in
      let file_line file line =
        if String.length file > 0 && file.[0] <> '<' then
          try
            let lines = In_channel.read_lines file in 
            List.nth lines (line - 1)
          with _ -> 
            None
        else None 
      in
      let style = ANSITerminal.[Bold; red] in
      eprintf "%s%!" @@ ANSITerminal.sprintf style 
        "[ERROR] %s error: %s\n" kind msg;
      List.iteri pos_lst ~f:(fun i pos ->
        let Ast.Pos.{ file; line; col; len } = pos in
        match file_line file line with
        | Some file_line  ->
          let pre = if i = 0 then "" else "then in\n        " in 
          eprintf "%s%!" @@ ANSITerminal.sprintf style "        %s%s: %d,%d\n" 
            pre file line col;
          eprintf "%s%!" @@ ANSITerminal.sprintf style "   %3d: %s" 
            line (String.prefix file_line col);
          eprintf "%s%!" @@ ANSITerminal.sprintf 
            ANSITerminal.[Bold; white; on_red] "%s" 
            (String.sub file_line ~pos:col ~len);
          eprintf "%s%!" @@ ANSITerminal.sprintf style "%s" 
            (String.drop_prefix file_line (col + len));
          eprintf "%s%!" @@ ANSITerminal.sprintf [] "\n"
        | None -> ()
      );

      exit 1
