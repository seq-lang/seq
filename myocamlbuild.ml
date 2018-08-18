open Ocamlbuild_plugin
open Command

let static_lib_loc = "../clib/libmain.a"

let () = dispatch begin function
  | After_rules -> 
    pdep ["link"] "linkdep" (fun param -> [param]);
    flag ["link"; "ocaml"; "native"]
      (S[A"-cclib"; A"-L../clib -lmain"]);
    flag ["link"; "ocaml"; "byte"]
      (S[A"-cclib"; A"-L../clib -lmain"])
  | _ -> ()
end
