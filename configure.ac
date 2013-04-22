
# process this file with autoconf to produce a configure script.

AC_INIT(animenu,0.3.99)
AM_INIT_AUTOMAKE()

# checks for programs
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL

# checks for headers / library files.
LIBS="$LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS -lm -lpthread"
disable_xft="no"
AC_ARG_ENABLE(xft,
              [  --disable-xft            disable use of xft library for translucency],
              disable_xft="yes")
disable_icons="no"
AC_ARG_ENABLE(icons,
              [  --disable-icons          disable use of xpm library for icons],
              disable_icons="yes")
AC_HEADER_STDC
AC_CHECK_HEADERS(pthread.h fcntl.h malloc.h sys/ioctl.h sys/time.h unistd.h lirc/lirc_client.h)
AC_PATH_X
if test x$no_x = "xyes"; then
  AC_MSG_ERROR("Need X11 library!")
else
  CFLAGS="$CFLAGS -I${ac_x_includes} -L${ac_x_libraries}" 
fi
if test x$disable_xft = "xno"; then
  AC_CHECK_LIB(Xft, XftDrawRect,,AC_DEFINE(NOXFT,1))
fi
if test x$disable_icons = "xno"; then
  AC_CHECK_LIB(Xpm, XpmCreatePixmapFromData)
fi
AC_CHECK_LIB(pthread, pthread_create,,AC_MSG_ERROR("Need pthread library!"))
AC_CHECK_LIB(lirc_client, lirc_init,,AC_MSG_ERROR("Need lirc_client library!"))

LIBS="$LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS -lm -lpthread"

AC_SUBST(PACKAGE_NAME)
AC_SUBST(CC)
AC_SUBST(include, "include \$(top_srcdir)/Makefile.include")

AC_CONFIG_FILES(Makefile.include src/Makefile Makefile)
AC_OUTPUT()

