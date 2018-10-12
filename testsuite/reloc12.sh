#!/bin/bash
. `dirname $0`/functions.sh
rm -f reloc12 reloc12lib*.so reloc12.log
rm -f prelink.cache
$CC -shared -O2 -fpic -o reloc12lib1.so $srcdir/reloc12lib1.c
$CC -shared -O2 -fpic -o reloc12lib2.so $srcdir/reloc12lib2.c
BINS="reloc12"
LIBS="reloc12lib1.so reloc12lib2.so"
$CCLINK -o reloc12 $srcdir/reloc12.c -Wl,--rpath-link,. ${LIBS}
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./reloc12 > reloc12.log
$PRELINK ${PRELINK_OPTS--vm} ./reloc12 >> reloc12.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` reloc12.log && exit 2
if [ "x$CROSS" = "x" ]; then
 LD_LIBRARY_PATH=. ./reloc12 || exit 3
fi
readelf -a ./reloc12 >> reloc12.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./reloc12
comparelibs >> reloc12.log 2>&1 || exit 5
