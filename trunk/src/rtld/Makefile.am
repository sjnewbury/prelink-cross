## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnu

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
INCLUDES = -I$(top_srcdir)/src @GELFINCLUDE@

sbin_PROGRAMS = prelink-rtld

prelink_rtld_SOURCES = $(top_srcdir)/src/data.c $(top_srcdir)/src/dso.c \
		       $(top_srcdir)/src/canonicalize.c $(top_srcdir)/src/wrap-file.c \
		       $(top_srcdir)/src/reloc-info.c $(top_srcdir)/src/reloc-info.h \
		       ld-libs.c ld-libs.h \
		       ld-lookup.c ld-lookup64.c ld-do-lookup.h

prelink_rtld_LDADD = @LIBGELF@ -liberty
prelink_rtld_CFLAGS = -DDSO_READONLY
prelink_rtld_LDFLAGS =
