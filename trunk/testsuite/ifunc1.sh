#!/bin/bash
. `dirname $0`/functions.sh
# First check if __thread is supported by ld.so/gcc/ld/as:
$RUN_HOST $CCLINK -o ifunctest $srcdir/ifunctest.c -Wl,--rpath-link,. > /dev/null 2>&1 || exit 77
( $RUN LD_LIBRARY_PATH=. ./ifunctest || { rm -f ifunctest; exit 77; } ) 2>/dev/null || exit 77
rm -f ifunctest ifunc1 ifunc1lib*.so ifunc1.log
rm -f prelink.cache
$RUN_HOST $CC -shared -O2 -fpic -o ifunc1lib1.so $srcdir/ifunc1lib1.c
$RUN_HOST $CC -shared -O2 -fpic -o ifunc1lib2.so $srcdir/ifunc1lib2.c ifunc1lib1.so
BINS="ifunc1"
LIBS="ifunc1lib1.so ifunc1lib2.so"
$RUN_HOST $CCLINK -o ifunc1 $srcdir/ifunc1.c -Wl,--rpath-link,. ifunc1lib2.so ifunc1lib1.so
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./ifunc1 >> ifunc1.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./ifunc1 >> ifunc1.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` ifunc1.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./ifunc1 || exit 3
fi
$RUN_HOST $READELF -a ./ifunc1 >> ifunc1.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./ifunc1
comparelibs >> ifunc1.log 2>&1 || exit 5
