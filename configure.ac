dnl Process this file with autoconf to produce a configure script.

define(VERSION_NUM,[0.0.051103])
define(VERSION_STR,["Silent Bot"])

define(TARGET,[gbot])
define(ADDR,[jose@ssanchez.net])


dnl Init
dnl ---------------------------------------------------------------------

dnl PACKAGE_NAME PACKAGE_TARNAME, PACKAGE_STRING, PACKAGE_VERSION
AC_INIT(TARGET, VERSION_NUM, ADDR)

AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(build)

AM_CONFIG_HEADER(src/config.h)

# PACKAGE, VERSION
AM_INIT_AUTOMAKE

dnl Checks for programs.
dnl ---------------------------------------------------------------------

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

AC_PREREQ(2.52)

CFLAGS="-D_GNU_SOURCE -std=c99"

dnl Checks for libraries.
dnl ---------------------

dnl Checks for header files.
dnl ---------------------------------------------------------------------
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
dnl ---------------------------------------------------------------------
AC_TYPE_SIZE_T

dnl Checks for os specific
dnl ---------------------------------------------------------------------
AC_PATH_PROG(unamepath, uname)
if test "$unamepath" == "" ; then
	system="unknown"
else
	system=`$unamepath -s`
fi

case "$system" in 
	Linux)
		CFLAGS="$CFLAGS -pthread"
		OS="Linux ($system `uname -r`)"
		AC_DEFINE(OS, LINUX, [OS Type])
		;;
	SunOS)
		CFLAGS="$CFLAGS -D__EXTENSIONS__ -lsocket -lnsl -pthreads"
		OS="Sun ($system `uname -r`)"
		AC_DEFINE(OS, SUN, [OS Type])
		;;

	dnl not (yet) tested in latests versions...
	Darwin)
		OS="Darwin ($system `uname -r`)"
		AC_DEFINE(OS, DARWIN, [OS Type])
		;;
	FreeBSD)
		CFLAGS="$CFLAGS -I/usr/local/include -L/usr/local/lib -std=c99"
		OS="FreeBSD ($system `uname -r`)"
		AC_DEFINE(OS, BSD, [OS Type])
		;;
	OpenBSD | NetBSD)
		CFLAGS="$CFLAGS -I/usr/local/include -L/usr/local/lib"
		OS="BSD ($system `uname -r`)"
		AC_DEFINE(OS, BSD, [OS Type])
		;;
	CYGWIN*)
		OS="Cygwin ($system `uname -r`)"
		AC_DEFINE(OS, CYGWIN, [OS Type])
		;;
	*)
		AC_MSG_CHECKING([if TARGET will build on $system])
		AC_MSG_RESULT(maybe)
		if test "$system" == "unknown" ; then
			OS="unknown"
		else
			OS="unknown ($system `$unamepath -r`)"
		fi
		AC_DEFINE(OS, UNKNOWN, [OS Type])
		;;
esac


if ! test "$system" == "unknown" ; then
	case "`uname -m`" in
		i486*)
			CFLAGS="$CFLAGS -march=i486"
			;;
		i586*)
			CFLAGS="$CFLAGS -march=i586"
			;;
		i686*)
			CFLAGS="$CFLAGS -march=i686"
			;;
	esac
fi

dnl Checks for library functions.
dnl ---------------------------------------------------------------------
AC_FUNC_FNMATCH
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRFTIME

AC_CHECK_FUNCS([dup2 gethostbyname memchr memset mkdir socket strchr strstr strtol snprintf strncasecmp])

m4_include([build/acx_pthread.m4])
ACX_PTHREAD([has_pthreads="yes"], [has_pthreads="no"])

dnl Symbols
dnl ---------------------------------------------------------------------

AC_DEFINE(VERSIONSTR, VERSION_STR, [Version codename])

AC_ARG_ENABLE(debug, 
	AC_HELP_STRING([--enable-debug],[Turn on debugging]))
AC_ARG_ENABLE(profiling, 
	AC_HELP_STRING([--enable-profiling],[Turn on profiling]))
AC_ARG_ENABLE(nodaemon,
	AC_HELP_STRING([--enable-nodaemon],[Prevent the bot to fork()]))
AC_ARG_ENABLE(warnings, 
	AC_HELP_STRING([--enable-warnings],[Turn on compiler warnings]))
AC_ARG_ENABLE(fnmatch, 
	AC_HELP_STRING([--enable-fnmatch],[Uses fnmatch instead of regexes (deprecated) for the events text]))
AC_ARG_ENABLE(pthreads, 
	AC_HELP_STRING([--enable-pthreads],[Enables or disables the use of posix threads (if available).]))

if test "$has_pthreads" == "yes" -a "$enable_pthreads" != "no"; then
	AC_DEFINE(HAVE_PTHREAD, 1, [Sets or unsets posix threads.])
	threads_enabled="yes"
else
	threads_enabled="no"
fi


if test "$enable_profiling" == "yes"; then
	CFLAGS="$CFLAGS -pg"
	enable_debug="no"
	enable_warnings="yes"
else
	enable_profiling="no"
fi

AM_CONDITIONAL(DEBUG, test x$enable_debug = xyes)


# When profiling, disable debug, seems to cause problems (the program doesn't work).
if test "$enable_debug" == "yes"; then
dnl	AC_DEFINE(DEBUG, 1, [Debug])
dnl	Changed to make unnecessary include config.h in some files
	CFLAGS="$CFLAGS -g3 -DDEBUG"
	enable_nodaemon="yes"
	enable_warnings="yes"
else
	if test "$enable_profiling" == "no"; then
		CFLAGS="$CFLAGS -O3"
	fi
	enable_debug="no"
fi

if test "$enable_nodaemon" != "yes"; then
  	AC_DEFINE(RUN_AS_DAEMON, 1, [Comment for a non demonized bot])
	DAEMON="yes"
else
	DAEMON="no"
fi

if test "$enable_warnings" == "yes"; then
	CFLAGS="$CFLAGS -pedantic -Wall"
fi

dnl this just enables fnmatch for text patterns on events, 
dnl fnmatch is always used to match channels and masks
if test "$enable_fnmatch" == "yes"; then
	CFLAGS="$CFLAGS -DUSE_FNMATCH"
	MATCH="fnmatch"
else
	MATCH="regexes"
fi




dnl Create files.
dnl -------------

AC_OUTPUT([
Makefile
src/Makefile
scripts/Makefile
])

AC_MSG_RESULT([
    TARGET has been configured successfully.

	OS                 : $OS
	Compiler           : $CC
	Compiler flags     : $CFLAGS
	Daemon             : $DAEMON

	Debug              : $enable_debug
	Profiling          : $enable_profiling

	Match system       : $MATCH
	Using threads      : $threads_enabled
])

