#!/usr/bin/env bash
# Path: compile.sh
# Author: Matteo Spanio <spanio at dei.unipd.it>
# Script to compile SoX from source on Linux

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
END='\033[0m'
BOLD='\033[1m'
RED='\033[0;31m'

# generate configure script
echo -e $YELLOW $BOLD "Generating configure script..." $END
autoreconf --install

# create build directory
mkdir -p build
mkdir -p bin
cd build

# run configure script
echo -e $YELLOW $BOLD "Running configure script..." $END
../configure --disable-shared

# compile
echo -e $YELLOW $BOLD "Compiling..." $END
make

# copy executable to bin directory
cp src/sox ../bin
cp src/play ../bin
cp src/rec ../bin
cp src/soxi ../bin

echo -e $GREEN "SoX compiled successfully!" $END
echo "You can now run SoX by typing ./bin/sox"