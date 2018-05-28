OCB_FLAGS :=  
OCB := corebuild $(OCB_FLAGS)
TARGET := main

.PHONY: 
	all clean byte native profile debug sanity test

all:
	$(OCB) $(TARGET).native

cpp:
	$(OCB) -verbose 1 bindings.o

debug:
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
	opam switch 4.06.1

install-ocaml-packages:
	opam update
	opam install \
		async core_extended core_bench \  
		cohttp async_graphics cryptokit menhir \
		llvm camlp4 batteries ctypes-foreign ppx_deriving
