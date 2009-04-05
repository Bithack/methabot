AC_INIT(lmm_file, 0.1.0, http://bithack.se/projects/methabot/docs/modules/lmm_file/)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(src/config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

LT_INIT([disable-static])
AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
[  --enable-debug           Enable debugging output],
	debug=true, debug=false)
if test "$debug" = "true"; then
  CFLAGS="${CFLAGS} -DDEBUG -g -O0"
fi

AC_PATH_PROG(METHA_CONFIG, metha-config)
if test -f "${METHA_CONFIG}"; then
    LDFLAGS="`${METHA_CONFIG} --libs` $LDFLAGS"
    CFLAGS="`${METHA_CONFIG} --cflags` $CFLAGS"
else
    AC_MSG_ERROR([metha-config not found])
fi

AC_PATH_PROG(METHA_CONFIG, metha-config)
if test -f "${METHA_CONFIG}"; then
    LDFLAGS="`${METHA_CONFIG} --libs` $LDFLAGS"
    CFLAGS="`${METHA_CONFIG} --cflags` $CFLAGS"
else
    AC_MSG_ERROR([metha-config not found])
fi

AC_CHECK_FUNCS([strdup realloc strncasecmp memmem])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([metha/metha.h])

libdir=$libdir/metha/modules

AC_OUTPUT([
Makefile
src/Makefile
])
