#! /bin/bash

debugFlags=$1
insts=$2

# build gem5
# scons -j 4 ./build/ALPHA_MESI_CMP_directory/gem5.opt

# simulate
./build/ALPHA_MESI_CMP_directory/gem5.opt --debug-flag=$1 configs/spec2k6/run.py -b sjeng --cpu-type=inorder --caches --maxinsts=$2 --l1i_size=512B --l1d_size=512B --l1d_assoc=2 --l2cache --l2_size=512B --l2_assoc=6 --opt_l2 --sc_assoc=2
