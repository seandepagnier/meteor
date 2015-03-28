LDADD = ../libmeteor/.libs/libmeteor.la
INCLUDES = -I../libmeteor

noinst_PROGRAMS = hello rwerrors

hello_SOURCES = hello.c
rwerrors_SOURCES = rwerrors.c

if HAVE_LIBGLUT
noinst_PROGRAMS += glut
glut_SOURCES = glut.c
endif
