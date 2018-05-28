libmain.a: main.o
	ar rsc libmain.a main.o

main.o: main.c
	g++ --std=c++11 -c main.c -o main.o

clean:
	rm main.o libmain.a