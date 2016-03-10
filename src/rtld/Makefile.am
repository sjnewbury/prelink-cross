## Process this file with automake to create Makefile.in

PKGVERSION = "\"@PKGVERSION@\""
REPORT_BUGS_TO = "\"@REPORT_BUGS_TO@\""

DEFS = -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wno-pointer-sign
AM_CFLAGS = -Wall -Wno-pointer-sign
AM_CPPFLAGS = -DSBINDIR='"@sbindir@"' -DBINDIR='"@bindir@"' \
            -DEXECSTACK_PROG="\"`echo execstack | sed '$(transform)'`\"" \
            -DPRELINK_PROG="\"`echo prelink | sed '$(transform)'`\"" \
            -DPRELINK_RTLD_PROG="\"`echo prelink-rtld | \
                                    sed '$(transform)'`\"" \
	    -DEXEEXT='"$(EXEEXT)"' \
	    -DPKGVERSION=$(PKGVERSION) \
	    -DREPORT_BUGS_TO=$(REPORT_BUGS_TO)
AM_CPPFLAGS += -I$(top_srcdir)/src @GELFINCLUDE@

sbin_PROGRAMS = prelink-rtld

prelink_rtld_SOURCES = ../data.c ../dso.c \
		       ../canonicalize.c ../wrap-file.c \
		       ../reloc-info.c ../src/reloc-info.h \
		       rtld.c ld-libs.h \
		       dl-hash.h dl-object.c dl-load.c \
		       dl-tls.c dl-version.c dl-misc.c \
		       dl-lookup.c dl-lookupX.h

prelink_rtld_LDADD = @LIBGELF@ -liberty
prelink_rtld_CFLAGS = -DDSO_READONLY
prelink_rtld_LDFLAGS =
