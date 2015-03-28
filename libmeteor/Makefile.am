lib_LTLIBRARIES = libmeteor.la
libmeteor_la_SOURCES = mesh.c fileio.c mem.c data.c matrix.c heap.c build.c kdtree.c *.h
include_HEADERS = meteor.h

libmeteor_la_LDFLAGS = -version-info 0:2:0

EXTRA_DIST = tetracalc.c term-optimizer.scm infix2prefix.scm

AM_CFLAGS = $(LIBMESH_CFLAGS)
LIBS = -lm
