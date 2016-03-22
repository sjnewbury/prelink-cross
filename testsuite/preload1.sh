#!/bin/bash
. `dirname $0`/functions.sh
check_one() {
  cnt=0
  log=$1
  echo -n . >> preload1.log
  text="$2"
  shift 2
  while [ $# -gt 0 ]; do
    grep -q "^$text .*$1" $log || exit 40
    cnt=$((++cnt))
    shift
  done
  [ `grep "^$text " $log | wc -l` = $cnt ] || exit 41
}
check_log() {
  log=$1
  echo -n "Checking $1 " >> preload1.log
  check_one $log 'Checking executable' $CHECKE
  check_one $log 'Checking shared library' $CHECKL
  check_one $log 'Assuming prelinked' $ASSUME
  check_one $log 'Prelinking' $PREL
  echo >> preload1.log
}

PRELINK=`echo $PRELINK \
	 | sed -e 's, \./\(prelink\.\(cache\|conf\)\), preload1.tree/etc/\1,g' \
	       -e 's,path=\.,path=preload1.tree/lib:preload1.tree/usr/lib,' \
	       -e 's,linker=\./,linker=preload1.tree/lib/,'`
CCLINK=`echo $CCLINK \
	| sed -e 's,linker=\./,linker=preload1.tree/lib/,'`
rm -rf preload1.tree
rm -f preload1.log
mkdir -p preload1.tree/{lib,etc,usr/lib,usr/bin}
$CC -shared -O2 -fpic -o preload1.tree/usr/lib/lib1.so $srcdir/preload1lib1.c
$CC -shared -O2 -fpic -o preload1.tree/usr/lib/lib2.so $srcdir/preload1lib2.c
for lib in `cat syslib.list`; do
  cp -p $lib.orig preload1.tree/lib/$lib
  cp -p $lib.orig preload1.tree/lib/$lib.orig
done
for lib in `cat syslnk.list`; do
  cp -dp $lib preload1.tree/lib
done
$CCLINK -o preload1.tree/usr/bin/bin1 $srcdir/preload1.c \
    -Wl,--rpath-link,preload1.tree/usr/lib -L preload1.tree/usr/lib -lc -l1
cat > preload1.tree/etc/prelink.conf <<EOF
preload1.tree/usr/bin
preload1.tree/lib
preload1.tree/usr/lib
EOF
LIBS="preload1.tree/usr/lib/lib1.so preload1.tree/usr/lib/lib2.so"
LIBS="$LIBS `sed 's|^|preload1.tree/lib/|' syslib.list`"
BINS="preload1.tree/usr/bin/bin1"
savelibs
chmod 644 `ls $BINS | sed 's|$|.orig|'`
# Make sure prelinked binaries and libraries will have different ctimes
# than mtimes
sleep 3s
echo $PRELINK ${PRELINK_OPTS--vm} -avvvvv > preload1.log
$PRELINK ${PRELINK_OPTS--vm} -avvvvv > preload1.tree/etc/log1 2>&1 || exit 1
cat preload1.tree/etc/log1 >> preload1.log
echo $PRELINK ${PRELINK_OPTS--vm} -aqvvvvv >> preload1.log
$PRELINK ${PRELINK_OPTS--vm} -aqvvvvv > preload1.tree/etc/log2 2>&1 || exit 2
cat preload1.tree/etc/log2 >> preload1.log
# We -expect- a failure here!
LD_LIBRARY_PATH=preload1.tree/lib:preload1.tree/usr/lib preload1.tree/usr/bin/bin1 && exit 3
for f in $LIBS $BINS ; do
  cp $f $f.orig
done
echo $PRELINK ${PRELINK_OPTS--vm} --ld-preload=preload1.tree/usr/lib/lib2.so -avvvvv >> preload1.log
$PRELINK ${PRELINK_OPTS--vm} --ld-preload=preload1.tree/usr/lib/lib2.so -avvvvv > preload1.tree/etc/log4 2>&1 || exit 4
cat preload1.tree/etc/log4 >> preload1.log
# System libs and lib1.so MIGHT change, but lib2.so and bin1 must change
for i in preload1.tree/usr/lib/lib2.so preload1.tree/usr/bin/bin1; do
  cmp -s $i.orig $i && exit 5
done
# Should run, but fail (no preload)
LD_LIBRARY_PATH=preload1.tree/lib:preload1.tree/usr/lib preload1.tree/usr/bin/bin1 && exit 6
# Should run, and exit successfully
LD_PRELOAD=preload1.tree/usr/lib/lib2.so LD_LIBRARY_PATH=preload1.tree/lib:preload1.tree/usr/lib preload1.tree/usr/bin/bin1 || exit 7
chmod 755 $BINS
exit 0
