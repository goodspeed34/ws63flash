AM_CPPFLAGS = -I$(top_srcdir)/lib

bin_PROGRAMS = ws63flash ws63fwpkg
ws63flash_SOURCES = ws63flash.c
ws63flash_LDADD = $(top_builddir)/lib/libgnu.a

ws63fwpkg_SOURCES = ws63fwpkg.c
ws63fwpkg_LDADD = $(top_builddir)/lib/libgnu.a

dist_noinst_HEADERS = ws63defs.h io.h ymodem.h fwpkg.h baud.h
