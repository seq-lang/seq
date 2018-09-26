all:
	make -j ../build
	LIBRARY_PATH=`pwd`/../build dune build src/main.exe
	ln -s -f _build/default/src/main.exe .
	
clean:
	dune clean

install-dep:
	brew install ocaml opam 
	opam init
	opam update
	opam install core core_extended core_bench dune ctypes ctypes-foreign ansiterminal menhir ppx_deriving
