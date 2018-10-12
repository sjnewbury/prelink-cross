#!/bin/bash
. `dirname $0`/functions.sh
rm -f reloc12 reloc12lib*.so reloc12.log
rm -f prelink.cache
$RUN_HOST $CC -shared -O2 -fpic -o reloc12lib1.so $srcdir/reloc12lib1.c
$RUN_HOST $CC -shared -O2 -fpic -o reloc12lib2.so $srcdir/reloc12lib2.c
if [ $? -ne 0 ]; then
 echo "tested relocation not available in this GCC/Linker"
 exit 77
fi
BINS="reloc12"
LIBS="reloc12lib1.so reloc12lib2.so"
$RUN_HOST $CCLINK -o reloc12 $srcdir/reloc12.c -Wl,--rpath-link,. ${LIBS}
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./reloc12 > reloc12.log
$RUN_HOST $PRELINK ${PRELINK_OPTS--vm} ./reloc12 >> reloc12.log 2>&1 || exit 1
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` reloc12.log && exit 2
if [ "x$CROSS" = "x" ]; then
 $RUN LD_LIBRARY_PATH=. ./reloc12 || exit 3
fi
$RUN_HOST $READELF -a ./reloc12 >> reloc12.log 2>&1 || exit 4
# So that it is not prelinked again
chmod -x ./reloc12
comparelibs >> reloc12.log 2>&1 || exit 5
