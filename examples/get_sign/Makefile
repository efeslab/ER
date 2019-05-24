.DEFAULT_GOAL := default
CC=clang-6.0
CFLAGS=-I ../../include -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone
BYTECODE=get_sign.bc
klee.bc: get_sign.c
	${CC} ${CFLAGS} -DKLEE_SYMBOLIC $< -o $@

noklee.bc: get_sign.c
	${CC} ${CFLAGS} $< -o $@

ARGS=

include ../common/Makefile.inc
