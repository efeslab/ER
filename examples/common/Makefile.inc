default: klee.bc noklee.bc
KLEE_CMD= klee \
		-write-kqueries \
		-write-paths \
		--libc=uclibc \
		--posix-runtime \
		-env-file=env_file \
		-output-source=false \
		-ignore-posix-path=true \
		-pathrec-entry-point="__klee_posix_wrapped_main" \
		-output-stats=false \
		-output-istats=false
		#-write-sym-paths \
		#-solver-backend=z3 \
		#-write-cons-paths \
		#-write-stats-paths \
		#-use-query-log=solver:kquery \
		#-debug-z3-dump-queries=z3queries \
		#-debug-z3-dump-queries-stats \
		#-debug-z3-log-api-interaction=z3-api
ifneq ($(KLEE_PATHREC),)
	KLEE_CMD += -pathrec-entry-point $(KLEE_PATHREC)
endif
ifneq ($(KLEE_IGNORE_POSIX),)
	KLEE_CMD += -ignore-posix-path=true
endif
KLEE_REPLAY?=000001
KLEE_OUT_DIR?=record_symlink
.PHONY: record_exec record replay clean fullsym
env_file:
	set > env_file
record_exec: noklee.bc env_file
	cp noklee.bc ${BYTECODE}
	${KLEE_CMD} ${BYTECODE} ${ARGS}
record_symlink: klee-last
	ln -sfT $(realpath klee-last) record_symlink
record: record_exec record_symlink
	@echo Record done!

replay: record_symlink klee.bc env_file
	cp klee.bc ${BYTECODE}
	${KLEE_CMD} --replay-path=${KLEE_OUT_DIR}/test${KLEE_REPLAY}.path ${BYTECODE} ${ARGS}

fullsym: klee.bc
	cp klee.bc ${BYTECODE}
	${KLEE_CMD} ${BYTECODE} ${ARGS}
clean:
	-rm -rf klee-out-* klee-last klee.bc noklee.bc ${BYTECODE} record_symlink
help:
	@echo "make" by default generates klee.bc and noklee.bc depending on the MACRO KLEE_SYMBOLIC
	@echo use "make record" to run klee with noklee.bc to get path of concrete execution
	@echo use "make replay" to run klee with klee.bc to replay concrete path during symbolic execution
