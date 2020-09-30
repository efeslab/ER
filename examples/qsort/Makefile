.DEFAULT_GOAL := default
CC=clang-8
CFLAGS=-I ../../include -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone
BYTECODE=quicksort.bc
klee.bc: quicksort.c
	${CC} ${CFLAGS} -DKLEE_SYMBOLIC $< -o $@

noklee.bc: quicksort.c
	${CC} ${CFLAGS} $< -o $@

ARGS= < stdin

include ../common/Makefile.inc
