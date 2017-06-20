#!/bin/bash
. `dirname $0`/functions.sh
# Disable this test under SELinux
if test -x /usr/sbin/getenforce; then
  case "`/usr/sbin/getenforce 2>/dev/null`" in
    Permissive|Disabled) ;;
    *) exit 77 ;;
  esac
fi
rm -f reloc8 reloc8lib*.so reloc8.log
rm -f prelink.cache
NOCOPYRELOC=-Wl,-z,nocopyreloc
case "`uname -m`" in
  x86_64|s390*|sparc*) if file reloc1lib1.so | grep -q 64-bit; then NOCOPYRELOC=; fi;;
esac
$CC -shared -O2 -Wl,-z,nocombreloc -fpic -o reloc8lib1.so $srcdir/reloc3lib1.c
$CC -shared -O2 -Wl,-z,nocombreloc -fpic -o reloc8lib2.so $srcdir/reloc1lib2.c reloc8lib1.so
BINS="reloc8"
LIBS="reloc8lib1.so reloc8lib2.so"
$CCLINK -o reloc8 $NOCOPYRELOC $srcdir/reloc7.c -Wl,--rpath-link,. reloc8lib2.so -lc reloc8lib1.so
LD_LIBRARY_PATH=. ./reloc8 > reloc8.log || exit 1
readelf -a ./reloc8 >> reloc8.log 2>&1 || exit 2
savelibs
echo $PRELINK ${PRELINK_OPTS--vm} ./reloc8 >> reloc8.log
$PRELINK ${PRELINK_OPTS--vm} ./reloc8 >> reloc8.log 2>&1 || exit 3
grep -q ^`echo $PRELINK | sed 's/ .*$/: /'` reloc8.log && exit 4
LD_LIBRARY_PATH=. ./reloc8 >> reloc8.log || exit 5
readelf -a ./reloc8 >> reloc8.log 2>&1 || exit 6
# So that it is not prelinked again
chmod -x ./reloc8
comparelibs >> reloc8.log 2>&1 || exit 7
