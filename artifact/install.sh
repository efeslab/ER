#!/bin/sh
set -e
WORKDIR=/
# docker run --name 'test' -itd ubuntu:18.04 /bin/bash
# docker exec -ti test /bin/bash
apt-get update
apt-get install -y llvm-8 clang-8 vim build-essential bash-completion git cmake python3 python3-setuptools wget tmux

### get ER
cd $WORKDIR
echo Downloading ER
git clone 'https://github.com/efeslab/ER.git'
cd ER
git checkout pldi21-artifact
git submodule update --init

### Z3
echo Installing Z3
cd $WORKDIR/ER/third-party/z3
python3 scripts/mk_make.py
cd build
make -j `nproc` install

### minisat
echo Installing minisat
apt-get install -y zlib1g-dev
cd $WORKDIR/ER/third-party/minisat
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j`nproc` install

### cryptominisat
echo Installing Cryptominisat
cd $WORKDIR/ER/third-party/cryptominisat
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j`nproc` install

### STP
echo Installing STP
apt-get install -y cmake bison flex libboost-all-dev python perl
cd $WORKDIR/ER/third-party/stp
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j`nproc` install

### klee-uclibc
echo Installing klee-uclibc
cd $WORKDIR/ER/third-party/klee-uclibc
apt-get install -y libncurses5-dev
./configure --make-llvm-lib --with-llvm-config=`which llvm-config-8`
KLEE_CFLAGS="-DKLEE_SYM_PRINTF" CC=clang-8 make -j`nproc`

### klee
echo Installing klee
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

### wllvm
echo Installing wllvm
apt-get install -y python3-pip
pip3 install wllvm

### bugbasev2
echo Installing Dependencies for bugbasev2
#sqlite: tcl-dev
apt-get install -y tcl-dev
cd ${WORKDIR}/ER/third-party/bugbasev2
make -C sqlite-7be932d all-bc
make -C sqlite-4e8e485 all-bc
make -C sqlite-787fa71 all-bc
make -C php-74194 all-bc
