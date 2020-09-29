
avid.bc:
	seqc -o avid.bc avid.seq

avid.o: avid.bc
	llc avid.bc -O3 -filetype=obj -o avid.o

avid: avid.o FORCE
	clang -lseqrt -lomp -ldl -lz -O3 -o avid avid.o

FORCE: