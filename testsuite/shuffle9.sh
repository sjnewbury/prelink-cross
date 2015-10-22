#!/bin/bash
. `dirname $0`/functions.sh
# Kernels before 2.4.10 are known not to work
case "`$RUN uname -r`" in
  [01].*|2.[0-3].*|2.4.[0-9]|2.4.[0-9][^0-9]*) exit 77;;
esac
rm -f shuffle9 shuffle9.log
BINS="shuffle9"
$RUN_HOST $CCLINK -o shuffle9 $srcdir/shuffle9.c -Wl,--rpath-link,. shuffle3lib2.so -lc shuffle3lib1.so
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./shuffle9 > shuffle9.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./shuffle9 >> shuffle9.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` shuffle9.log && exit 2
$RUN LD_LIBRARY_PATH=. ./shuffle9 || exit 3
$RUN_HOST $READELF -a ./shuffle9 >> shuffle9.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./shuffle9
comparelibs >> shuffle9.log 2>&1 || exit 5
