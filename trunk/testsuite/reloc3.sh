#!/bin/bash
. `dirname $0`/functions.sh
rm -f reloc3 reloc3lib*.so reloc3.log
rm -f prelink.cache
$CC -shared -O2 -fpic -o reloc3lib1.so $srcdir/reloc3lib1.c
$CC -shared -O2 -fpic -o reloc3lib2.so $srcdir/reloc1lib2.c reloc3lib1.so
BINS="reloc3"
LIBS="reloc3lib1.so reloc3lib2.so"
$CCLINK -o reloc3 $srcdir/reloc3.c -Wl,--rpath-link,. reloc3lib2.so reloc3lib1.so
$STRIP -g $BINS $LIBS
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./reloc3 > reloc3.log
$PRELINK ${PRELINK_OPTS--vm} ./reloc3 >> reloc3.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` reloc3.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./reloc3 >> reloc3.log || exit 3
fi
$READELF -a ./reloc3 >> reloc3.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./reloc3
comparelibs >> reloc3.log 2>&1 || exit 5
