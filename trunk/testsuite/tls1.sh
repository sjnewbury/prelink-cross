#!/bin/bash
. `dirname $0`/functions.sh
# First check if __thread is supported by ld.so/gcc/ld/as:
rm -f tlstest
#echo '__thread int a; int main (void) { return a; }' \
#  | $RUN_HOST $CCLINK -xc - -o tlstest > /dev/null 2>&1 || exit 77
#( $RUN LD_LIBRARY_PATH=. ./tlstest || { rm -f tlstest; exit 77; } ) 2>/dev/null || exit 77
rm -f tls1 tls1lib*.so tls1.log
rm -f prelink.cache
$RUN_HOST $CC -shared -O2 -fpic -o tls1lib1.so $srcdir/tls1lib1.c
$RUN_HOST $CC -shared -O2 -fpic -o tls1lib2.so $srcdir/tls1lib2.c tls1lib1.so
BINS="tls1"
LIBS="tls1lib1.so tls1lib2.so"
$RUN_HOST $CCLINK -o tls1 $srcdir/tls1.c -Wl,--rpath-link,. tls1lib2.so tls1lib1.so
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./tls1 > tls1.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./tls1 >> tls1.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` tls1.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./tls1 || exit 3
fi
$RUN_HOST $READELF -a ./tls1 >> tls1.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./tls1
comparelibs >> tls1.log 2>&1 || exit 5
