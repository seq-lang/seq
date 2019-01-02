(******************************************************************************
 *
 * Seq OCaml 
 * jupyter.ml: Jupyter kernel bindings
 *
 * Author: inumanag
 * Based on https://github.com/KKostya/simple_jucaml/blob/master/jucaml.ml
 *
 ******************************************************************************)

open Core

module J  = Yojson.Basic
module JU = Yojson.Basic.Util

module Z = ZMQ

let json fields = 
  J.(to_string @@ `Assoc fields)

let wrap_capture callback f =
  let open Unix in
  
  let flush_all () =
    Core.flush_all ()
    [@@ocaml.warning "-3"]
  in
  
  let fd = openfile "capture" 
    ~mode:[ O_RDWR; O_TRUNC; O_CREAT ] 
    ~perm:0o600 
  in
  let tmp_cout, tmp_cerr = dup Unix.stdout, dup Unix.stderr in
  dup2 ~src:fd ~dst:stdout;
  dup2 ~src:fd ~dst:stderr;
  let reset () =
    flush_all ();
    dup2 ~src:tmp_cout ~dst:Unix.stdout;
    dup2 ~src:tmp_cerr ~dst:Unix.stderr;
  in
  let result = 
    try f () 
    with ex -> begin
      reset (); 
      close fd; 
      print_endline "(kernel) wrap_capture exception here";
      flush_all ();
      raise ex
    end 
  in
  reset ();
  let sz = Int64.to_int_exn @@ (fstat fd).st_size in
  let buffer = Bytes.create sz in
  let _ = lseek fd 0L ~mode:SEEK_SET in
  let _ = read fd ~buf:buffer ~pos:0 ~len:sz in
  close fd;
  callback @@ Bytes.to_string buffer;
  result 

let exec jit code callback =
  let r = wrap_capture callback (fun () -> 
    begin try 
      Jit.exec jit code
    with Err.CompilerError (typ, pos_lst) ->
      Err.print_error typ pos_lst ~file:code
    end;
    "") 
  in
  callback r

module WireIO = struct
  type t = 
    { key: Cstruct.t;
      uuid: string;
      kerneldir: string }

  let create kerneldir key = 
    { key = Cstruct.of_string key; 
      uuid = Uuid.(to_string @@ create ());
      kerneldir }
  
  type wire_msg = 
    { header: string;
      parent_header: string;
      metadata: string;
      content: string;
      extra: string list }
  
  let msg_to_list msg =
    [ msg.header; msg.parent_header; msg.metadata; msg.content ] @ msg.extra

  let to_string msg = 
    J.(to_string @@ `List (List.map ~f:(fun x -> `String x) @@ msg_to_list msg))

  let sign t msg = 
    msg 
     |> msg_to_list
     |> List.map ~f:Cstruct.of_string 
     |> Cstruct.concat
     |> Nocrypto.Hash.mac `SHA256 ~key:(t.key)  
     |> Hex.of_cstruct
     |> function `Hex x -> x  

  let mk_message t ?(metadata="{}") ?(extra=[]) htype parent_header content =
    let zone = Lazy.force Time.Zone.local in 
    let header = json [
      "date",     `String Time.(to_filename_string ~zone @@ now ());
      "msg_id",   `String Uuid.(to_string @@ create ());
      "username", `String "kernel";
      "session",  `String t.uuid;
      "msg_type", `String htype;
      "version",  `String "1.0"]
    in 
    { header; parent_header; content; metadata; extra }

  let read_msg t socket =
    let rec scan zmqids = function
      | "<IDS|MSG>" :: signature :: h :: p :: m :: c :: e -> 
        let msg = 
          { header = h; 
            parent_header = p; 
            metadata = m; 
            content = c; 
            extra = e }
        in 
        zmqids, signature, msg
      | zmqid :: tl -> 
        scan (zmqid :: zmqids) tl
      | _ -> 
        failwith "Malformed wire message" 
    in
    let message = Z.Socket.recv_all socket in
    let zmqids, signature, msg = scan [] message in
    if (sign t msg) <> signature then
      failwith @@ sprintf "Received a message with wrong signature: %s vs %s"
        (sign t msg) signature
    else
      zmqids, msg

  let send_msg t socket ?(zmqids=[]) msg =
    let lmsg = "<IDS|MSG>" :: sign t msg :: msg_to_list msg in
    Z.Socket.send_all socket (zmqids @ lmsg)
end

let counter = ref 0 ;;

let handler jit wireio iopub mtype = 
  let content msg key = 
    msg.WireIO.content |> J.from_string |> JU.member key |> JU.to_string
  in
  let send_to_iopub msg = function
    | "" -> 
      print_endline @@ sprintf "(kernel) ignoring reply" 
        (* (WireIO.to_string msg) *)
    | reply ->
      let content = json [
        "name", `String "stdout"; 
        "text", `String reply]
      in
      print_endline @@ sprintf "(kernel) content: %s" content;
      let rmsg = WireIO.mk_message wireio "stream" msg.WireIO.header content in
      WireIO.send_msg wireio iopub rmsg
  in
  let reply_kernel_info msg = 
    Filename.concat wireio.kerneldir "kernel_info.json"
     |> J.from_file
     |> J.to_string
  in
  let reply_comm msg = json [
    "comm_id", `String (content msg "comm_id");
    "target_name", `String (content msg "target_name");
    "data", `Assoc []]  
  in
  let reply_execute msg =
    counter := !counter + 1;
    let code = content msg "code" in
    exec jit code @@ send_to_iopub msg;
    json [ 
      "status", `String "ok"; 
      "execution_count", `Int !counter]
  in
  print_endline @@ sprintf "received %s" mtype;
  match mtype with
  | "kernel_info_request" -> 
    "kernel_info_reply", reply_kernel_info 
  | "comm_open" -> 
    "comm_close", reply_comm    
  | "execute_request" -> 
    "execute_result", reply_execute   
  | "is_complete_request" ->
    "is_complete_reply", fun _ -> json [ "status", `String "complete" ]
  | e ->
    "unknown", fun _ -> json []

let handle jit wireio iopub socket =
  let zmqids, msg = WireIO.read_msg wireio socket in
  let mtype = 
    msg.WireIO.header 
     |> J.from_string 
     |> JU.member "msg_type" 
     |> JU.to_string 
  in
  let rtype, handler = handler jit wireio iopub mtype in
  let content = handler msg in 
  let rmsg = WireIO.mk_message wireio rtype msg.WireIO.header content in
  WireIO.send_msg wireio socket ~zmqids rmsg

let jupyter kerneldir settings_json =
  let jit = Jit.init () in

  (* Processing Jupyter-kernel settings file *)
  let settings_str k = 
    J.from_file settings_json |> JU.member k |> JU.to_string 
  in
  let settings_int k = 
    J.from_file settings_json |> JU.member k |> JU.to_int  
  in

  (* Setting up WireIO module *)
  let wireio = settings_str "key" |> WireIO.create kerneldir in

  (* Firing up ZMQ sockets *)
  let open Z in
  let context = Context.create () in
  let hb      = Socket.create context Socket.rep   
  and shell   = Socket.create context Socket.router  
  and control = Socket.create context Socket.router  
  and stdin   = Socket.create context Socket.router  
  and iopub   = Socket.create context Socket.pub   in
  let addr = sprintf "%s://%s:%d" 
    (settings_str "transport") (settings_str "ip") 
  in
  settings_int "hb_port"     |> addr |> Socket.bind hb;
  settings_int "shell_port"  |> addr |> Socket.bind shell;
  settings_int "control_port"|> addr |> Socket.bind control;
  settings_int "stdin_port"  |> addr |> Socket.bind stdin;
  settings_int "iopub_port"  |> addr |> Socket.bind iopub;

  (* Creating poller *)
  let poller = Poll.
    (mask_of [| hb, In; shell, In; control, In; stdin, In |])
  in
  (* Entering polling loop *)
  let handle = handle jit wireio iopub in
  while true do
    let evts = Poll.poll poller in
    List.iteri [ hb; shell; control; stdin ] ~f:(fun i socket -> 
      match i, evts.(i) with
      | _, None   -> 
        ()
      | 1, Some _ -> 
        handle shell 
      | n, Some _ ->
        print_string ("Received event on socket #" ^ string_of_int i ^"\n");
        Socket.recv_all socket |> String.concat ~sep:"\n" |> print_string;
        Out_channel.flush stdout)
  done
