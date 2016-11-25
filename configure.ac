AC_INIT(methabot, 1.8.0, https://www.bithack.com/)

AM_INIT_AUTOMAKE([subdir-objects])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AM_CONFIG_HEADER(src/config.h)
AM_MAINTAINER_MODE
AM_ICONV

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PROG_LIBTOOL

build_cli=yes

AC_ARG_ENABLE(debug,
[  --enable-debug           Enable debug output for libmetha],
	debug=yes, debug=no)
AC_ARG_ENABLE(io-debug,
[  --enable-io-debug        Enable I/O debug output for libmetha],
	io_debug=yes, io_debug=no)

AM_CONDITIONAL(SLAVE, test x$build_slave = xyes)
AM_CONDITIONAL(MASTER, test x$build_master = xyes)
AM_CONDITIONAL(CLI, test x$build_cli = xyes)
AM_CONDITIONAL(CLIENT, test x$build_client = xyes)

if test "x$debug" = "xyes"; then
  CFLAGS="${CFLAGS} -Wall -DDEBUG -g -O0"
fi
if test "x$io_debug" = "xyes"; then
  CFLAGS="${CFLAGS} -DIO_DEBUG -g"
fi

AC_PATH_PROG(CURL_CONFIG, curl-config)
if test -f "${CURL_CONFIG}"; then
    LDFLAGS="`${CURL_CONFIG} --libs` $LDFLAGS"
    CFLAGS="`${CURL_CONFIG} --cflags` $CFLAGS"
else
    AC_MSG_WARN([curl-config not found, guessing build settings])
fi

if test "x${MINGW32}" = "xyes"; then
    AC_CHECK_LIB([nspr4],[main],,AC_MSG_ERROR([unable to link with nspr4]))
    AC_CHECK_LIB([wsock32],[main],,AC_MSG_ERROR([unable to link with wsock32]))
    LIBS="${LIBS} -lnspr4 -lwsock32 "
fi

CFLAGS="${CFLAGS} -D_GNU_SOURCE"

AC_CHECK_HEADER([curl/curl.h],,AC_MSG_ERROR([unable to find libcurl headers]))
AC_CHECK_LIB([curl],[curl_global_init],,AC_MSG_ERROR([unable to link with libcurl]))

PTHREAD_LIBS=error
AC_CHECK_LIB(pthread, pthread_attr_init, PTHREAD_LIBS="-lpthread")
if test "x$PTHREAD_LIBS" = xerror; then
   AC_CHECK_LIB(pthreads, pthread_attr_init, PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
   AC_CHECK_LIB(c_r, pthread_attr_init, PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
   AC_CHECK_LIB(pthreadGC2, pthread_attr_init, PTHREAD_LIBS="-lpthreadGC2")
fi
if test "x$PTHREAD_LIBS" = xerror; then
    AC_MSG_ERROR([unable to find a usable pthreads library, this is required!])
fi
LIBS="${PTHREAD_LIBS} ${LIBS}"

AC_CHECK_HEADERS([pthread.h stdint.h sys/epoll.h])
AC_CHECK_FUNCS([strdup realloc strncasecmp memmem epoll_ctl])

AC_DEFINE_UNQUOTED(BUILD_LIBS, "${LIBS}", [used by modules to link the same libraries as libmetha])
AC_DEFINE_UNQUOTED(BUILD_CFLAGS, " -D_GNU_SOURCE -DXP_UNIX ${BUILD_FLAGS_EXTRA}", [used by modules to build with the same flags at libmetha])

AC_MSG_CHECKING([if your compiler supports __sync_add_and_fetch])

AC_TRY_LINK(,
[
  int v = 1;
  return (__sync_add_and_fetch(&v, 1) == 2 ? 0 : 1);
],
[
  AC_MSG_RESULT([yes])
  AC_DEFINE(HAVE_BUILTIN_ATOMIC, 1, [Define 1 if your version of gcc supports __sync_add_and_fetch])
],
[
  AC_MSG_RESULT(no)
  AC_DEFINE(HAVE_BUILTIN_ATOMIC, 0, [Define 1 if your version of gcc supports __sync_add_and_fetch])
]
)

includedir=$includedir/metha

AC_OUTPUT([
src/Makefile
src/libmethaconfig/Makefile
src/libmetha/Makefile
src/methabot/Makefile
src/metha-config/Makefile
Makefile
])

echo
echo ----
echo
echo "Configure of methabot-"AC_PACKAGE_VERSION", done at `date`:"
echo 
echo "  Host:                       $host"
echo "  Installation prefix:        $prefix"
echo "  Building mb:                $build_cli"
echo "  Debugging enabled:          $debug"
echo "  I/O-debugging enabled:      $io_debug"
echo
echo "Run 'make' to compile."
echo 

