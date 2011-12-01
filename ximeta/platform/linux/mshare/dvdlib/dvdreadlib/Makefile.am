INCLUDES = -I$(top_srcdir) @CSS_CFLAGS@

lib_LTLIBRARIES = libdvdread.la
libdvdread_la_SOURCES = dvd_input.c dvd_reader.c dvd_udf.c ifo_read.c \
	ifo_print.c nav_read.c nav_print.c bswap.h dvd_udf.h dvd_input.h \
	dvdread_internal.h \
	md5.c md5.h
libdvdread_la_LIBADD = @DL_LIBS@ @CSS_LIBS@
libdvdread_la_LDFLAGS = -version-info 3:0:0 -export-symbols-regex "(DVD.*|ifo.*|nav.*)"

pkgincludedir = $(includedir)/dvdread

pkginclude_HEADERS = dvd_reader.h ifo_types.h ifo_read.h \
	ifo_print.h nav_types.h nav_read.h nav_print.h
