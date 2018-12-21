OCAML_DIR = $(shell ocamlc -where)
 -include  $(OCAML_DIR)/Makefile.config

SEQ_BUILD_DIR=../build
BUILD_DIR=build

all:
	rm -f main.exe
	ln -s -f $(SEQ_BUILD_DIR)/libseq.dylib .
	dune build --build-dir $(BUILD_DIR) --profile macos-dev main.exe main.so
	ln -s -f $(BUILD_DIR)/default/main.exe .
	ln -s -f $(BUILD_DIR)/default/main.so libparser.dylib
	
clean:
	dune clean --build-dir $(BUILD_DIR)

