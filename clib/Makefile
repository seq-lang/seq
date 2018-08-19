libmain.a: main.o caml.o format.o
	ar rsc libmain.a main.o caml.o format.o

shared: main.o caml.o format.o
	g++ -g -dynamiclib main.o caml.o format.o \
		-o libmain.dylib \
		-L cpp/build -lseq \
		-lgc \
		-L /usr/local/Cellar/ocaml/4.07.0/lib/ocaml -lcamlrun_shared

main.o: main.cc
	g++ -fPIC -g --std=c++11 -c main.cc -o main.o

format.o: format.cc
	g++ -fPIC -g --std=c++11 -c format.cc -o format.o

caml.o: caml.cc
	g++ -fPIC -g --std=c++11 \
		-I /usr/local/Cellar/ocaml/4.07.0/lib/ocaml \
		-I /usr/local/Cellar/llvm/6.0.1/include \
		-I cpp/src \
		-I cpp/include \
		-c caml.cc -o caml.o

clean:
	rm main.o libmain.a