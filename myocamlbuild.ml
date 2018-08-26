open Ocamlbuild_plugin
open Command

let () = dispatch @@ function
  | After_rules -> 
    pdep ["link"] "linkdep" (fun param -> [param]);
    flag ["link"; "ocaml"; "native"] (S[A"-cclib"; A"-L.. -lseq"]);
    flag ["link"; "ocaml"; "byte"]   (S[A"-cclib"; A"-L.. -lseq"])
  | _ -> ()
