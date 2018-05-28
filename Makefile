OCB_FLAGS :=  
OCB := corebuild $(OCB_FLAGS)
TARGET := main

.PHONY: 
	all clean byte native profile debug sanity test

all: cpp
	$(OCB) $(TARGET).byte

cpp:
	make -C clib

debug: cpp
	$(OCB) $(TARGET).byte -tag debug 
	# OCAMLRUNPARAM=b ./$(TARGET).byte <(echo 'load "test.fq" | print')

clean:
	$(OCB) -clean

test: all
	$(OCB) -I test test.native
	./test.native
	./toy.native test.txt 2>&1 | grep -e error -e ERROR || echo ok

install-osx:
	brew install ocaml opam llvm
	opam init

install-dep:
	opam update
	opam install \
		async core_extended core_bench \  
		cohttp async_graphics cryptokit menhir \
		llvm camlp4 batteries ctypes-foreign ppx_deriving
