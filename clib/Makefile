libmain.a: main.o
	ar rsc libmain.a main.o

main.o: main.cc
	g++ --std=c++11 -c main.cc -o main.o

clean:
	rm main.o libmain.a