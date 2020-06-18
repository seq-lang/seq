MENHIR?=menhir
MENHIR_LIB?=`ocamlfind query menhirLib`
OCAML?=ocamlopt
OCAMLLEX?=ocamllex

all: parser.o
	
parser.o: lex menhir
	$(OCAML) \
		-w -40 -g \
		-fPIC -runtime-variant _pic \
		-strict-sequence \
		-strict-formats \
		-short-paths \
		-keep-locs \
		-output-complete-obj \
		-o seqparser.o \
		-I $(MENHIR_LIB) \
		menhirLib.cmx \
		ast.ml seqgrammar.ml lexer.ml seqgrammar_messages.ml main.ml

lex:
	$(OCAMLLEX) -q -o lexer.ml lexer.mll

menhir:
	$(MENHIR) \
		--compile-errors seqgrammar.messages \
		seqgrammar.mly \
		> seqgrammar_messages.ml
	$(MENHIR) \
		--base seqgrammar \
		--table seqgrammar.mly
	rm -f seqgrammar.mli

clean:
	rm -f \
		ast.{o,cmi,cmx} \
		lexer.{ml,o,cmi,cmx} \
		main.{o,cmi,cmx} \
		seqgrammar_messages.{ml,o,cmi,cmx} \
		seqgrammar.{ml,mli,o,cmi,cmx} \
		seqparser.o
