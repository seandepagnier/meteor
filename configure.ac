AC_PREREQ(2.59)

AC_INIT([meteor], 0.7, [sean@depagnier.com])
AM_INIT_AUTOMAKE(meteor, 0.7)
AC_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AM_PROG_LIBTOOL

# Checks for libraries
AC_CHECK_LIB(m, sqrt, , AC_MSG_ERROR([*** meteor requires libm]))
AC_CHECK_LIB(ltdl, lt_dlopen, , have_ltdl=no)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_HEADER_TIME
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
#AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memset strchr strerror])

dnl set variables for support option args
AC_ARG_ENABLE(glut,
  [  --enable-glut         build with glut support for displaying meshes],,
  enable_glut=yes)
AC_ARG_ENABLE(osmesa,
  [  --enable-osmesa       build with osmesa support for rendering meshes],,
  enable_osmesa=yes)   
AC_ARG_ENABLE(png,
  [  --enable-png          build with png support for loading textures, and render output],,
  enable_png=yes)   
AC_ARG_ENABLE(ffmpeg,
  [  --enable-ffmpeg       build with ffmpeg support for generating videos],,
  enable_ffmpeg=yes)

AC_ARG_ENABLE(debug,
  [  --enable-debug        build with debugging support, no optimizations,
and extra sanity checking],, enable_debug=no)

AC_ARG_WITH(libgc,
  [  --with-libgc        build with libgc memory support],,
  with_libgc=no)
AC_ARG_WITH(float-format,
[  --with-float-format=<format>  valid formats are: (float, double, long double)
default is double], float_format=$withval, float_format=double)

dnl Test for gl
if test "$enable_glut" = "yes" || test "$enable_osmesa" = "yes"; then
   AC_CHECK_LIB([GL], [glViewport], , enable_glut=no enable_osmesa=no)
fi

dnl Test for glu
if test "$enable_glut" = "yes" || test "$enable_osmesa" = "yes"; then
   AC_CHECK_LIB([GLU], [gluPerspective], , enable_glut=no enable_osmesa=no)
fi

dnl Test for glut
if test "$enable_glut" = "yes"; then
   AC_CHECK_HEADER(GL/glut.h, 
      AC_CHECK_LIB([glut], [glutInit], , enable_glut=no),
   enable_glut=no)
fi

AM_CONDITIONAL(HAVE_LIBGLUT, test $enable_glut = yes)

dnl Test for osmesa
if test "$enable_osmesa" = "yes"; then
   AC_CHECK_HEADER(GL/osmesa.h, 
      AC_CHECK_LIB([OSMesa], [OSMesaCreateContext], , enable_osmesa=no),
    enable_osmesa=no)
fi

dnl disable png and ffmpeg support if there is no opengl
if test "$enable_glut" = "no" && test "$enable_osmesa" = "no"; then
   if test "$enable_png" = "yes" || test "$enable_ffmpeg" = "yes"; then
      AC_MSG_WARN([with no opengl output support, png and ffmpeg support is not useful])
      enable_png=no
      enable_ffmpeg=no
   fi
fi

dnl Test for png
if test "$enable_png" = "yes"; then
   AC_CHECK_HEADER(png.h,
      AC_CHECK_LIB([png], [png_create_read_struct], , enable_png=no),
   enable_png=no)
fi

dnl Test for ffmpeg
if test "$enable_ffmpeg" = "yes"; then
  AC_CHECK_LIB([z], [compress],
   [AC_CHECK_HEADER(ffmpeg/avformat.h, [AC_DEFINE([HAVE_FFMPEG], [], [ ])
                    LIBS="$LIBS-lavformat -lavcodec -lavutil -lz "], enable_ffmpeg=no)],
                    enable_ffmpeg=no)
fi

dnl Test for libgc
if test "$with_libgc" = "yes"; then
   AC_CHECK_LIB(gc, GC_malloc, , with_libgc=no)
fi

dnl Test for debug
if test "$enable_debug" = "yes"; then
   CFLAGS="-g -DDEBUG"
else
   LIBMESH_CFLAGS=" -funroll-loops"
   AC_SUBST(LIBMESH_CFLAGS)
fi

dnl defines for floating point format
if test "$float_format" = "float"; then
   AC_DEFINE_UNQUOTED([USE_FLOAT_FORMAT],, [floating point format set to float])
elif test "$float_format" = "double"; then
   AC_DEFINE_UNQUOTED([USE_DOUBLE_FORMAT],, [floating point format set to double])
elif test "$float_format" = "long double"; then
   AC_DEFINE_UNQUOTED([USE_LONG_DOUBLE_FORMAT],, [floating point format set to long double])
else
   AC_MSG_ERROR([unrecognized floating point format: $float_format])
fi

AC_CONFIG_FILES([
Makefile
man/Makefile
libmeteor/Makefile
demos/Makefile
src/Makefile
])

AC_OUTPUT

AC_MSG_RESULT([
Build options:
  Version                   $VERSION
  Install prefix            $prefix
  CFLAGS                    $CFLAGS
  LDFLAGS                   $LDFLAGS
  LIBS                      $LIBS
 
Building Support For:
  glut           $enable_glut
  osmesa         $enable_osmesa
  png            $enable_png
  ffmpeg         $enable_ffmpeg

Float format: $float_format
Use libgc for garbage collection: $with_libgc
Debugging Support: $enable_debug]);

dnl warn about osmesa having no way to output
if test "$enable_osmesa" = "yes"; then
   if test "$enable_png" = "no" && test "$enable_ffmpeg" = "no"; then
      AC_MSG_WARN([osmesa enabled, but without png or ffmpeg it cannot generate output])
   fi
fi

dnl warn about not having libtool
if test "$have_ltdl" = "no"; then
   AC_MSG_WARN([meteor needs libtool to load functions!])
fi
