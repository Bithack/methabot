AC_INIT(lmm_mysql, 1.0.0, http://bithack.se/projects/methabot/docs/modules/lmm_mysql/)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(src/config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
[  --enable-debug           Enable debugging output],
	debug=true, debug=false)
if test "$debug" = "true"; then
  CFLAGS="${CFLAGS} -DDEBUG -g -O0"
fi

AC_ARG_WITH(mysql-include,
[  --with-mysql-include     Optional path to where mysql.h is],
    mc_inc="$withval",
    mc_inc="")

AC_ARG_WITH(mysql-lib,
[  --with-mysql-lib         Optional path to where mysql libraries],
    mc_lib="$withval",
    mc_lib="")

if test "x$mc_inc" != "x"; then
    CFLAGS="${CFLAGS} -I${mc_inc}"
else
    CFLAGS="${CFLAGS} -I/usr/include/mysql"
fi

AC_CHECK_HEADERS([mysql.h],,
        AC_MSG_ERROR([could not find mysql.h]))

if test "x$mc_lib" != "x"; then
    LDFLAGS="${LDFLAGS} -L${mc_lib}"
fi

AC_CHECK_LIB(mysqlclient_r, mysql_real_connect,,AC_MSG_ERROR(could not link libmysqlclient_r))
LIBS="-lmysqlclient_r"

AC_PATH_PROG(METHA_CONFIG, metha-config)
if test -f "${METHA_CONFIG}"; then
    LDFLAGS="`${METHA_CONFIG} --libs` $LDFLAGS"
    CFLAGS="`${METHA_CONFIG} --cflags` $CFLAGS"
else
    AC_MSG_ERROR([metha-config not found])
fi


AC_CHECK_FUNCS([strdup realloc strncasecmp memmem])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADER([metha/metha.h])

libdir=$libdir/metha/modules

AC_OUTPUT([
Makefile
src/Makefile
])
