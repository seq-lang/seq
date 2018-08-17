libmain.a: main.o caml.o
	ar rsc libmain.a main.o caml.o

main.o: main.cc
	g++ --std=c++11 -c main.cc -o main.o

caml.o: caml.cc
	g++ --std=c++11 \
		-I /usr/local/Cellar/ocaml/4.07.0/lib/ocaml \
		-I /usr/local/Cellar/llvm/6.0.1/include \
		-I cpp/src \
		-I cpp/include \
		-c caml.cc -o caml.o

clean:
	rm main.o libmain.a