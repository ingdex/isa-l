#!/bin/bash

cd `dirname $0`

mkdir -p usr/lib
mkdir -p usr/include

USR_DIR="`pwd`/usr"

#isa-l
if [ -d ./isa-l/.git ]; then
    pushd isa-l
    git pull
    popd
else
    git clone https://github.com/01org/isa-l.git
fi

pushd isa-l
./autogen.sh
./configure --prefix=$USR_DIR --libdir=$USR_DIR/lib
make
make install
popd
