OCAML_DIR = $(shell ocamlc -where)
 -include  $(OCAML_DIR)/Makefile.config

all:
	#make -j -C ../build
	ln -s -f ../build/libseq.dylib .
	dune build --build-dir build --profile macos src/main.exe src/main.so
	ln -s -f build/default/src/main.exe .
	ln -s -f build/default/src/main.so libparser.dylib

cpp:
	g++ -g -c -std=c++11 code.cpp -I "`ocamlc -where`" 
	LIBRARY_PATH="/usr/local/opt/libffi/lib:${LIBRARY_PATH}" \
		g++ -o code.exe \
		code.o \
		-lffi $(NATIVECCLIBS) \
		-L"`ocamlc -where`" -lasmrun \
		-L. -lparser \
		-L../build -lseq
	
clean:
	dune clean --build-dir build


install-dep:
	brew install ocaml opam 
	opam init
	opam update
	opam install core core_extended core_bench dune ctypes ctypes-foreign ansiterminal menhir ppx_deriving
