# Execution Reconstruction

This repo contains the implementation of the PLDI'21 paper "Reproducing Production Failures with Execution Reconstruction". Our implementation is based on [KLEE](https://klee.github.io/) v2.1 and llvm-8.

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

Please refer to our [artifact documentation](artifact/README.md) for installation and basic usage.