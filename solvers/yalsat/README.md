# YalSAT

This is yet another local search SAT solver.

To build run `./configure.sh && make`.  See also `./configure.sh -h`,
particularly the usage of `./configure.sh -g` to compile a version
with debugging, checking and logging support.

This will build both the library `libyals.a` with its API in file [yals.h](yals.h) and the stand-alone SAT solver `yalsat` and its multi-threaded parallel version `palsat`.


