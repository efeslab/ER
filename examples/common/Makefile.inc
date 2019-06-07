default: klee.bc noklee.bc
KLEE_CMD= klee -solver-backend=z3 \
		-write-kqueries \
		-write-paths \
		-write-sym-paths \
		--libc=uclibc \
		--posix-runtime \
		-write-query-stats \
		-write-query-change \
		-write-cons-paths \
		-write-stats-paths
		# -use-query-log=solver:kquery \
		#-debug-z3-dump-queries=z3queries \
		#-debug-z3-dump-queries-stats \
		#-debug-z3-log-api-interaction=z3-api

.PHONY: record_exec record replay clean fullsym
record_exec: noklee.bc
	cp noklee.bc ${BYTECODE}
	${KLEE_CMD} ${BYTECODE} ${ARGS}
record_symlink: klee-last
	ln -sfT $(realpath klee-last) record_symlink
record: record_exec record_symlink
	@echo Record done!

replay: record_symlink klee.bc
	cp klee.bc ${BYTECODE}
	${KLEE_CMD} --replay-path=record_symlink/test000001.path ${BYTECODE} ${ARGS}

fullsym: klee.bc
	cp klee.bc ${BYTECODE}
	${KLEE_CMD} ${BYTECODE} ${ARGS}
clean:
	-rm -rf klee-out-* klee-last klee.bc noklee.bc ${BYTECODE} record_symlink
help:
	@echo "make" by default generates klee.bc and noklee.bc depending on the MACRO KLEE_SYMBOLIC
	@echo use "make record" to run klee with noklee.bc to get path of concrete execution
	@echo use "make replay" to run klee with klee.bc to replay concrete path during symbolic execution
