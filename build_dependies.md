# Dependencies
You can find possible CMake parameters and their description in README-CMake.md
Not every solver is required to install.
E.g. I only installed STP and Z3, ignoring metaSMT (although only STP is used later).
And **DO NOT** forget to add your customized install dir to CMakeList.txt:
```
set(KLEE_COMPONENT_EXTRA_LIBRARIES "-L /mnt/storage/gefeizuo/hase/install/lib")
```

## STP
minisat is required.
### minisat
```
git clone https://github.com/niklasso/minisat
mkdir build
cmake -DCMAKE_INSTALL_PREFIX=/mnt/storage/gefeizuo/hase/install -DCMAKE_LIBRARY_PATH=/mnt/storage/gefeizuo/hase/install/lib -DCMAKE_INCLUDE_PATH=/mnt/storage/gefeizuo/hase/install/include ..
make install
```
After minisat is ready, you can compile stp:
```
mkdir build
# NOTE: use "ENABLE_PYTHON_INTERFACE=OFF" to disable python bindings, whose installation requres root (write to /usr/lib/python2.7/xxx)
cmake -DCMAKE_INSTALL_PREFIX=/mnt/storage/gefeizuo/hase/install -DCMAKE_LIBRARY_PATH=/mnt/storage/gefeizuo/hase/install/lib -DCMAKE_INCLUDE_PATH=/mnt/storage/gefeizuo/hase/install/include -DENABLE_PYTHON_INTERFACE=OFF ..
```
[cryptominisat](https://github.com/msoos/cryptominisat) can be installed as an optional extra
> STP uses minisat as its SAT solver by default but it also supports other SAT solvers including CryptoMiniSat as an optional extra.
I chose to install it.

## Z3
should be easy

## MetaSMT
This can be safely ignored. And its building process is tedious and its scripts are not well maintained recently.
Bellow are a few notes if you really want to try it.
When preparing dependencies of MetaSMT, it seems that authors does not maintain the bootstrap.sh recently.
There are a few hacks to compile it:
	1. change version of cvc. The provided "dependencies" git repo already updated cvc version to 4.1.6. So you need to change the cvc4.1.4 in bootstrap.sh to the cvc4.1.6
	2. When building cvc4.1.6, the BOOST_ROOT env somehow does not work well. You can manually specify "BOOST_ROOT=$PWD/deps/boost-1_55_0" to get around the boost Header Files error.
	3. You should use the "-j" option of the bootstrap.sh. You can also use MAKE='make -j40' to enable parallel compiling, if the previous one does not work well.
	4. cudd-2.4.2 is no longer on the internet (domain is down). Again, there are newer version in the external "dependencies" repo. You can try cudd-2.4.2-git.
	5. gcc-7 & g++-7 gave me a lot of warnings. So did gcc-5 & g++-5.
The bootstrap commandline looks like:
```
BOOST_ROOT=$PWD/deps/boost-1_55_0 ./bootstrap.sh -m RELEASE --free --install /mnt/storage/gefeizuo/hase/install --deps deps/ build/ -j40
```
