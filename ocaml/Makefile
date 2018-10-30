OCAML_DIR = $(shell ocamlc -where)
 -include  $(OCAML_DIR)/Makefile.config

SEQ_BUILD_DIR=../build
BUILD_DIR=build

all:
	#make -j -C ../build
	ln -s -f $(SEQ_BUILD_DIR)/libseq.dylib .
	dune build --build-dir $(BUILD_DIR) --profile macos-dev src/main.exe src/main.so
	ln -s -f $(BUILD_DIR)/default/src/main.exe .
	ln -s -f $(BUILD_DIR)/default/src/main.so libparser.dylib

cpp:
	g++ -g -c -std=c++11 code.cpp -I "`ocamlc -where`" 
	LIBRARY_PATH="/usr/local/opt/libffi/lib:${LIBRARY_PATH}" \
		g++ -o code.exe \
		code.o \
		-lffi $(NATIVECCLIBS) \
		-L"`ocamlc -where`" -lasmrun \
		-L. -lparser \
		-L$(SEQ_BUILD_DIR) -lseq
	
clean:
	dune clean --build-dir $(BUILD_DIR)


install-dep:
	brew install ocaml opam 
	opam init
	opam update
	opam install core core_extended core_bench \
		dune ctypes ctypes-foreign ansiterminal \
		menhir ppx_deriving
