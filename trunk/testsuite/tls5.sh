#!/bin/bash
. `dirname $0`/functions.sh
# First check if __thread is supported by ld.so/gcc/ld/as:
rm -f tlstest
echo '__thread int a; int main (void) { return a; }' \
  | $RUN_HOST $CCLINK -xc - -o tlstest > /dev/null 2>&1 || exit 77
if [ "x$CROSS" = "x" ]; then
 ( $RUN LD_LIBRARY_PATH=. ./tlstest || { rm -f tlstest; exit 77; } ) 2>/dev/null || exit 77
fi
rm -f tls5 tls5lib*.so tls5.log
rm -f prelink.cache
echo 'int tls5;' | $RUN_HOST $CC -shared -O2 -fpic -xc - -o tls5lib3.so
$RUN_HOST $CC -shared -O2 -fpic -o tls5lib1.so $srcdir/tls5lib1.c tls5lib3.so
$RUN_HOST $CC -shared -O2 -fpic -o tls5lib2.so $srcdir/tls1lib2.c \
  -Wl,--rpath-link,. tls5lib1.so
BINS="tls5"
LIBS="tls5lib1.so tls5lib2.so tls5lib3.so"
$RUN_HOST $CCLINK -o tls5 $srcdir/tls1.c -Wl,--rpath-link,. tls5lib2.so tls5lib1.so tls5lib3.so
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./tls5 > tls5.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./tls5 >> tls5.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` tls5.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./tls5 || exit 3
fi
$RUN_HOST $PRELINK -u tls5lib3.so || exit 4
echo $PRELINK ${PRELINK_OPTS--vm} ./tls5 >> tls5.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./tls5 >> tls5.log 2>&1 || exit 5
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` tls5.log && exit 6
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./tls5 || exit 7
fi
$RUN_HOST $READELF -a ./tls5 >> tls5.log 2>&1 || exit 8
# So that it is not prelinked again
chmod -x ./tls5
comparelibs >> tls5.log 2>&1 || exit 9
