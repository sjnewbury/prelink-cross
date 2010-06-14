## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnu

DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall
AM_CFLAGS = -Wall
AM_CPPFLAGS = -DSBINDIR='"@sbindir@"' -DBINDIR='"@bindir@"'
INCLUDES = @GELFINCLUDE@

sbin_PROGRAMS = prelink prelink-rtld
bin_PROGRAMS = execstack

arch_SOURCES = arch-i386.c arch-alpha.c arch-ppc.c arch-ppc64.c \
	       arch-sparc.c arch-sparc64.c arch-x86_64.c arch-mips.c \
	       arch-s390.c arch-s390x.c arch-arm.c arch-sh.c arch-ia64.c
common_SOURCES = checksum.c data.c dso.c dwarf2.c dwarf2.h fptr.c fptr.h     \
		 hashtab.c hashtab.h mdebug.c prelink.h stabs.c crc32.c      \
		 wrap-file.c canonicalize.c
prelink_SOURCES = cache.c conflict.c cxx.c doit.c exec.c execle_open.c get.c \
		  gather.c layout.c main.c prelink.c     \
		  prelinktab.h reloc.c reloc.h space.c undo.c undoall.c      \
		  verify.c md5.c md5.h sha.c sha.h 	     \
		  $(common_SOURCES) $(arch_SOURCES)
prelink_LDADD = @LIBGELF@ -liberty
prelink_LDFLAGS =

execstack_SOURCES = execstack.c $(common_SOURCES) $(arch_SOURCES)
execstack_LDADD = -liberty
execstack_LDFLAGS =

prelink_rtld_SOURCES = data.c dso-readonly.c ld-libs.c ld-lookup.c \
			canonicalize.c wrap-file.c ld-lookup64.c
prelink_rtld_LDADD = @LIBGELF@ -liberty
prelink_rtld_LDFLAGS =

extra_DIST = makecrc.c
