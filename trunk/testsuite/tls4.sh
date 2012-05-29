#!/bin/bash
. `dirname $0`/functions.sh
# First check if __thread is supported by ld.so/gcc/ld/as:
rm -f tlstest
#echo '__thread int a; int main (void) { return a; }' \
#  | $RUN_HOST $CCLINK -xc - -o tlstest > /dev/null 2>&1 || exit 77
#( $RUN LD_LIBRARY_PATH=. ./tlstest || { rm -f tlstest; exit 77; } ) 2>/dev/null || exit 77
rm -f tls4 tls4lib*.so tls4.log
rm -f prelink.cache
$RUN_HOST $CC -shared -O2 -fpic -o tls4lib1.so $srcdir/tls4lib1.c
$RUN_HOST $CC -shared -O2 -fpic -o tls4lib2.so $srcdir/tls4lib2.c \
  tls4lib1.so 2>/dev/null
BINS="tls4"
LIBS="tls4lib1.so tls4lib2.so"
$RUN_HOST $CCLINK -o tls4 $srcdir/tls4.c -Wl,--rpath-link,. tls4lib2.so tls4lib1.so
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./tls4 > tls4.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./tls4 >> tls4.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` tls4.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./tls4 || exit 3
fi
$RUN_HOST $READELF -a ./tls4 >> tls4.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./tls4
comparelibs >> tls4.log 2>&1 || exit 5
