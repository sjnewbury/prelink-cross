#!/bin/bash
CC="${CC:-gcc} ${LINKOPTS}"
CCLINK=${CCLINK:-${CC} -Wl,--dynamic-linker=`echo ./ld*.so.*[0-9]`}
CXX="${CXX:-g++} ${LINKOPTS}"
CXXLINK=${CXXLINK:-${CXX} -Wl,--dynamic-linker=`echo ./ld*.so.*[0-9]`}
PRELINK=${PRELINK:-../src/prelink -c ./prelink.conf -C ./prelink.cache --ld-library-path=. --dynamic-linker=`echo ./ld*.so.*[0-9]`}

# force deterministic timestamps to make double prelinking
# to produce unmodified binary.
PRELINK_TIMESTAMP=${PRELINK_TIMESTAMP:-12345678}
export PRELINK_TIMESTAMP

srcdir=${srcdir:-`dirname $0`}
savelibs() {
  for i in $LIBS $BINS; do cp -p $i $i.orig; done
}
comparelibs() {
  for i in $LIBS $BINS; do
    cp -p $i $i.new
    echo $PRELINK -u $i.new
    $PRELINK -u $i.new || exit
    cmp -s $i.orig $i.new || exit
    rm -f $i.new
    echo $PRELINK -y $i \> $i.new
    $PRELINK -y $i > $i.new || exit
    cmp -s $i.orig $i.new || exit
    rm -f $i.new
  done
}
