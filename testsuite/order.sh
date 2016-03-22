#!/bin/bash
. `dirname $0`/functions.sh
rm -f order orderlib*.so order.log
rm -f prelink.cache

# Need a sysroot for this...
$CC -shared -O2 -fpic -o orderlib3.so $srcdir/orderlib3.c
$CC -shared -O2 -fpic -o orderlib2.so $srcdir/orderlib2.c
$CC -shared -O2 -fpic -o orderlib1.so $srcdir/orderlib1.c orderlib3.so
$CC -shared -O2 -fpic -o orderlib.so $srcdir/orderlib.c orderlib1.so orderlib2.so orderlib3.so
BINS="order"
LIBS="orderlib.so orderlib1.so orderlib2.so orderlib3.so"
$CCLINK -o order $srcdir/order.c -Wl,-rpath-link,. orderlib.so orderlib3.so

: > order.log
LD_LIBRARY_PATH=. ./order >> order.log || exit 1
#exit 1
#savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./order >> order.log
$PRELINK ${PRELINK_OPTS--vm} ./order >> order.log 2>&1 || exit 2
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` order.log && exit 3
LD_LIBRARY_PATH=. ./order >> order.log || exit 4
# So that it is not prelinked again
chmod -x ./order
