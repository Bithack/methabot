AC_INIT(methabot, 1.7.0, http://bithack.se/projects/methabot/)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

m4_include([libev.m4])
AM_CONFIG_HEADER(src/config.h)
AM_MAINTAINER_MODE
AM_ICONV

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
[  --enable-debug           Enable debug output for libmetha],
	debug=true, debug=false)
AC_ARG_ENABLE(io-debug,
[  --enable-io-debug        Enable I/O debug output for libmetha],
	io_debug=true, io_debug=false)
AC_ARG_ENABLE(cli,
[  --disable-cli            Don't compile/install the command line tool],
    build_cli=false, build_cli=true)
AC_ARG_ENABLE(slave,
[  --enable-slave           Build the slave server daemon],
    build_slave=true, build_slave=false)
AC_ARG_ENABLE(master,
[  --enable-master          Build the master server daemon],
    build_master=true, build_master=false)
AC_ARG_ENABLE(client,
[  --enable-client          Build the client daemon],
    build_client=true, build_client=false)

if test "$debug" = "true"; then
  CFLAGS="${CFLAGS} -DDEBUG -g -O0"
fi
if test "$io_debug" = "true"; then
  CFLAGS="${CFLAGS} -DIO_DEBUG -g"
fi

AC_PATH_PROG(CURL_CONFIG, curl-config)
if test -f "${CURL_CONFIG}"; then
    LDFLAGS="`${CURL_CONFIG} --libs` $LDFLAGS"
    CFLAGS="`${CURL_CONFIG} --cflags` $CFLAGS"
else
    AC_MSG_WARN([curl-config not found, guessing build settings])
fi

case "$host" in
    *-*-mingw*) MINGW32=yes; JS_DEF="XP_WIN";;
    *) MINGW32=no; JS_DEF="XP_UNIX";;
esac

if test "x${MINGW32}" = "xyes"; then
    AC_CHECK_LIB([nspr4],[main],,AC_MSG_ERROR([unable to link with nspr4]))
    AC_CHECK_LIB([wsock32],[main],,AC_MSG_ERROR([unable to link with wsock32]))
    LIBS="${LIBS} -lnspr4 -lwsock32 "
fi

CFLAGS="${CFLAGS} -D${JS_DEF} -DJS_THREADSAFE -D_GNU_SOURCE"
CFLAGS_BKP="${CFLAGS}"
CFLAGS="${CFLAGS_BKP} -I/usr/include/js"
AC_CHECK_HEADER([js/jsapi.h],JS_INCLUDE_PATH="-I/usr/include/js",
    CFLAGS="${CFLAGS_BKP} -I/usr/include/mozjs"
    AC_CHECK_HEADER([mozjs/jsapi.h],JS_INCLUDE_PATH="-I/usr/include/mozjs",
        AC_MSG_ERROR([unable to find SpiderMonkey headers])
        , [#define ${JS_DEF} 1])
    , [#define ${JS_DEF} 1])
AC_CHECK_LIB([js],[JS_Now],,AC_CHECK_LIB([mozjs],[JS_Now],,AC_MSG_ERROR([unable to link with SpiderMonkey])))
CFLAGS="${CFLAGS_BKP} ${JS_INCLUDE_PATH}"

AC_MSG_CHECKING([whether to define MOZILLA_1_8_BRANCH])
AC_RUN_IFELSE([[
    /* determine whether the installed spidermonkey library
     * was built with MOZILLA_1_8_BRANCH defined */
    #include <jsapi.h>
    static JSBool f(JSContext *c, JSObject *t, uintN argc, jsval *args, jsval *r) {
        return JS_TRUE;
    }
    JSFunctionSpec fun[] = {
        {"test1", f, 10, JSFUN_HEAVYWEIGHT, 0},
        {"test2", f, 10, JSFUN_HEAVYWEIGHT, 0},
        {0},
    };
    int main() {
        JSRuntime *rt; JSContext *cx; JSObject *o; jsval v;
        rt = JS_NewRuntime(1024L*1024L*8L);
        cx = JS_NewContext(rt, 8192);
        o  = JS_NewObject(cx, 0, 0, 0);
        JS_SetGlobalObject(cx, o);
        JS_DefineFunctions(cx, o, fun); 
        if (JS_GetProperty(cx, o, "test2", &v) == JS_FALSE
            || !JSVAL_IS_OBJECT(v)
            || JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(v)) == JS_FALSE
            || JS_GetFunctionFlags(JS_ValueToFunction(cx, v)) != JSFUN_HEAVYWEIGHT)
            return 1;

        JS_DestroyContext(cx);
        JS_DestroyRuntime(rt);
        JS_ShutDown();
        return 0;
    }
    ]],AC_MSG_RESULT([no]),
[
    AC_MSG_RESULT([yes])
    CFLAGS="${CFLAGS} -DMOZILLA_1_8_BRANCH"
    BUILD_FLAGS_EXTRA="-DMOZILLA_1_8_BRANCH"
],
    ,AC_MSG_RESULT([unknown]))

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
LIBS="${PTHREAD_LIBS} ${LIBS} ${SPIDERMONKEY_LIBS}"

AC_CHECK_FUNCS([strdup realloc strncasecmp memmem])
AC_CHECK_HEADERS([pthread.h stdint.h])

AC_DEFINE_UNQUOTED(BUILD_LIBS, "${LIBS}", [used by modules to link the same libraries as libmetha])
AC_DEFINE_UNQUOTED(BUILD_CFLAGS, "${JS_INCLUDE_PATH} -D_GNU_SOURCE -DJS_THREADSAFE -DXP_UNIX ${BUILD_FLAGS_EXTRA}", [used by modules to build with the same flags at libmetha])

includedir=$includedir/metha

SUBDIRS_EXTRA=""

if test "x$build_slave" = "xtrue"; then
    SUBDIRS_EXTRA="mb-slaved"
fi
if test "x$build_master" = "xtrue"; then
    SUBDIRS_EXTRA="${SUBDIRS_EXTRA} mb-masterd"
fi
if test "x$build_client" = "xtrue"; then
    SUBDIRS_EXTRA="${SUBDIRS_EXTRA} mb-clientd"
fi

AC_SUBST(SUBDIRS_EXTRA)

AC_OUTPUT([
src/Makefile
src/libmethaconfig/Makefile
src/libmetha/Makefile
src/methabot/Makefile
src/metha-config/Makefile
src/mb-masterd/Makefile
src/mb-slaved/Makefile
src/mb-clientd/Makefile
Makefile
])

