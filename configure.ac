AC_INIT
AC_CONFIG_SRCDIR([./postf.c])
AM_INIT_AUTOMAKE(postf,1.0.01)
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
smr_WITH_BUILD_PATH
mni_REQUIRE_BICPL
AC_PATH_XTRA
LIBS="-lX11 -lXext $LIBS"
mni_REQUIRE_LIB(Ygl,[#include <X11/Ygl.h>],[winopen("");gconfig();])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT(epm-header)


