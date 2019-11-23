.DEFAULT_GOAL := default
CC=clang-6.0
CFLAGS=-I ../../include -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone
BYTECODE=main.bc
klee.bc: main.c
	${CC} ${CFLAGS} -DKLEE_SYMBOLIC $< -o $@

noklee.bc: main.c
	${CC} ${CFLAGS} $< -o $@
i2b: stdin_int_to_bin.c
	${CC} $< -o $@
binput: input
	./i2b < input > binput

ARGS= < binput

include ../common/Makefile.inc
