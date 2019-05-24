default: klee.bc noklee.bc
KLEE_CMD= klee -solver-backend=z3 \
		-debug-z3-dump-queries=z3queries \
		-debug-z3-dump-queries-stats \
		-write-kqueries \
		-write-paths \
		-write-sym-paths \
		--libc=uclibc \
		--posix-runtime \
		-use-query-log=solver:kquery \
		-write-query-stats \
		-write-query-change
		#-debug-z3-log-api-interaction=z3-api

.PHONY: record_exec record replay clean
record_exec: noklee.bc
	cp noklee.bc ${BYTECODE}
	${KLEE_CMD} ${BYTECODE} ${ARGS}
record_symlink: klee-last
	ln -sf $(realpath klee-last) record_symlink
record: record_exec record_symlink
	@echo Record done!

replay: record_symlink klee.bc
	cp klee.bc ${BYTECODE}
	${KLEE_CMD} --replay-path=record_symlink/test000001.path ${BYTECODE} ${ARGS}
clean:
	-rm -rf klee-out-* klee-last klee.bc noklee.bc ${BYTECODE} record_symlink
help:
	@echo `make` by default generates klee.bc and noklee.bc depending on the MACRO KLEE_SYMBOLIC
	@echo use `make record` to run klee with noklee.bc to get path of concrete execution
	@echo use `make replay` to run klee with klee.bc to replay concrete path during symbolic execution
