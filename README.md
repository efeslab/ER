# Execution Reconstruction(ER)

This repo contains the implementation of the PLDI'21 paper "Execution Reconstruction: Harnessing Failure Reoccurrences for Failure Reproduction". Our implementation is based on [KLEE](https://klee.github.io/) v2.1 and llvm-8.

2022-09-28: ported to work with llvm 14.0.6

## Description

ER is a hybrid failure reproduction tool utilizing symbolic execution and record/replay. At runtime, ER collects control-flow traces of reoccurring failures and incrementally traces selective data values everytime failure reoccurs. At offline, ER runs symbolic execution (KLEE) to gather path constraints of the failure-incurring input and reconstruct input data by constraint solving. When the path constraints become too complex for solver to reason, ER analyzes the constraint and instruct runtime data tracing to also record data which if known can simplify the complex constraint.

After such iterative procedures of online tracing and offline symbolic execution, ER generates the failure-incurring input which is guaranteed to reproduce the reoccurring failure.

## Components

1. Sherperded Symbolic Execution: modified based on KLEE.

   (1) [Symbolic Execution Engine](lib/)

   (2) [POSIX runtime](runtime/POSIX)

2. Key Data Value Selection:

   (1) KLEE Constraint Graph to DOT or JSON [converter](tools/kleaver)

   (2) Constraint Graph visualization: DOT viewer [Gephi](https://gephi.org/) (external tool), [Gephi python plugin for better visualization](utils/visualize/hase.py)

   (3) [Graph analysis script](utils/visualize/hase.py)

3. Data Recording (PTWrite) Instrumentation: [cmdline tool](tools/prepass), [instrumentation pass](lib/Module/PTWritePass.cpp)

4. Software Execution Trace: [examination tool](tools/pathviewer)

5. [Artifact and Docker image building instructions](artifact/)

## Build from source

The following build instructions are tested on Ubuntu 18.04. Assume your working directory is `${WORKDIR}`.

#### Install common prerequisite packages

```bash
apt-get install llvm-8 clang-8 build-essential git python3 cmake python3-setuptools wget tcl-dev
```

#### Build ER and dependnecies

Download source code

```bash
git clone 'https://github.com/efeslab/ER.git'
cd ER
git checkout pldi21-artifact
git submodule update --init
```

Install Z3

```bash
cd $WORKDIR/ER/third-party/z3
python3 scripts/mk_make.py
cd build
make -j `nproc` install
```

Install minisat

```bash
apt-get install -y zlib1g-dev
cd $WORKDIR/ER/third-party/minisat
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j`nproc` install
```

Install STP

```bash
apt-get install -y cmake bison flex libboost-all-dev python perl
cd $WORKDIR/ER/third-party/stp
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j`nproc` install
```

Install klee-uclibc

```bash
cd $WORKDIR/ER/third-party/klee-uclibc
apt-get install -y libncurses5-dev
./configure --make-llvm-lib --with-llvm-config=`which llvm-config-8`
KLEE_CFLAGS="-DKLEE_SYM_PRINTF" CC=clang-8 make -j`nproc`
```

Install wllvm

```bash
apt-get install -y python3-pip
pip3 install wllvm
```

Install ER

```bash
cd $WORKDIR/ER
apt-get install -y build-essential curl libcap-dev git cmake libncurses5-dev python-minimal python-pip unzip libtcmalloc-minimal4 libgoogle-perftools-dev libsqlite3-dev doxygen
mkdir build
cd build
LLVMPREFIX=/usr/lib/llvm-8/bin
cmake -DCMAKE_BUILD_TYPE=Release\
      -DKLEE_RUNTIME_BUILD_TYPE=Release\
      -DENABLE_SOLVER_STP=ON -DENABLE_SOLVER_Z3=ON\
      -DENABLE_POSIX_RUNTIME=ON\
      -DENABLE_SYSTEM_TESTS=OFF -DENABLE_UNIT_TESTS=OFF\
      -DENABLE_KLEE_UCLIBC=ON -DKLEE_UCLIBC_PATH=$WORKDIR/ER/third-party/klee-uclibc\
      -DLLVM_CONFIG_BINARY=${LLVMPREFIX}/llvm-config -DLLVMCC=${LLVMPREFIX}/clang -DLLVMCXX=${LLVMPREFIX}/clang++\
      ..
make -j`nprocs` install
```



## Usage

### Re-run the experiment in our paper

Please refer to our [artifact documentation](artifact/README.txt) for docker image installation and basic usage.

### Run arbitrary program with ER

Since ER is based on KLEE, a symbolic execution engine running on LLVM IR bitcode, you need to first compile the program into LLVM IR (e.g. using wllvm, please refer to [klee coreutils example](https://klee.github.io/tutorials/testing-coreutils/) for more information) and be able to run it concretly with KLEE's POSIX environment (using KLEE as a LLVM bitcode interpreter).
To reconstruct a failing execution, ER also requires an oracle failure-incurring input to simulate failure reoccurrences and iterative control-flow + data values tracing in KLEE.



## Special Thanks

- [Yiming Shi](https://github.com/syiming) analysed some KLEE constraint solving statistics and optimized the independent solver.
- [Yongwei Yuan](https://github.com/VictorYYW) and [Ruiyang Zhu](https://github.com/ry4nzhu) helped put together [a collection of software failures](https://github.com/efeslab/bugbasev2) for our evaluation.

