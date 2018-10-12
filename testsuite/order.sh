#!/bin/bash
. `dirname $0`/functions.sh
rm -f order orderlib*.so order.log
rm -f prelink.cache

# Need a sysroot for this...
$RUN_HOST $CC -shared -O2 -fpic -o orderlib3.so $srcdir/orderlib3.c
$RUN_HOST $CC -shared -O2 -fpic -o orderlib2.so $srcdir/orderlib2.c
$RUN_HOST $CC -shared -O2 -fpic -o orderlib1.so $srcdir/orderlib1.c orderlib3.so
$RUN_HOST $CC -shared -O2 -fpic -o orderlib.so $srcdir/orderlib.c orderlib1.so orderlib2.so orderlib3.so
BINS="order"
LIBS="orderlib.so orderlib1.so orderlib2.so orderlib3.so"
$RUN_HOST $CCLINK -o order $srcdir/order.c -Wl,-rpath-link,. orderlib.so orderlib3.so

: > order.log
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./order >> order.log || exit 1
fi
#exit 1
#savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./order >> order.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./order >> order.log 2>&1 || exit 2
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` order.log && exit 3
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./order >> order.log
 if [ $? -ne 0 ]; then
  echo "ERROR: Dynamic linker is resolving depth first, not breadth first"
  exit 4
 fi
fi
# So that it is not prelinked again
chmod -x ./order
