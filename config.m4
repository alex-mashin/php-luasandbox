dnl $Id$
dnl config.m4 for extension luasandbox

AC_PREREQ(2.50)

PHP_ARG_WITH(luasandbox, for luasandbox support,
[  --with-luasandbox             Include luasandbox support])

if test "$PHP_LUASANDBOX" != "no"; then
	dnl Include pkg-config macros definitions:
	m4_include([m4/pkg.m4])
	PKG_PROG_PKG_CONFIG

	dnl Under Debian the module is known as 'lua5.x', under FreeBSD as
	dnl 'lua-5.x', and some systems only provide an unversioned 'lua'.
	dnl Whichever matches first wins, so 5.1 is preferred where several
	dnl versions are installed side by side. The target Lua can also be pinned
	dnl explicitly with --with-luasandbox=MODULE (e.g. lua5.4). Detection
	dnl accepts any of Lua 5.1-5.4 inclusive; the version above is recorded in
	dnl $LUA_MODVERSION for logging and build output.
	if test "$PHP_LUASANDBOX" = "yes"; then
		LUA_MODULES="lua5.1 lua-5.1 lua51 lua5.2 lua-5.2 lua52 \
			lua5.3 lua-5.3 lua53 lua5.4 lua-5.4 lua54 lua"
	else
		LUA_MODULES="$PHP_LUASANDBOX"
	fi

	AC_MSG_CHECKING([for Lua])
	LUA_MODULE=
	for i in $LUA_MODULES; do
		if $PKG_CONFIG --exists "$i >= 5.1" "$i < 5.5" 2>/dev/null; then
			LUA_MODULE="$i"
			break
		fi
	done

	if test -z "$LUA_MODULE"; then
		AC_MSG_RESULT([not found])
		AC_MSG_ERROR([no pkg-config module for Lua 5.1-5.4 found (tried: $LUA_MODULES)])
	fi
	LUA_MODVERSION=`$PKG_CONFIG --modversion "$LUA_MODULE"`
	AC_MSG_RESULT([$LUA_MODULE $LUA_MODVERSION])

	PKG_CHECK_MODULES([LUA], [$LUA_MODULE])

	dnl Timers require real-time and pthread library on Linux and not
	dnl supported on other platforms
	AC_SEARCH_LIBS([timer_create], [rt], [
		PHP_EVAL_LIBLINE($LIBS, LUASANDBOX_SHARED_LIBADD)
	])
	AC_SEARCH_LIBS([sem_init], [pthread], [
		PHP_EVAL_LIBLINE($LIBS, LUASANDBOX_SHARED_LIBADD)
	])

	dnl LUA_LIBS and LUA_CFLAGS interprets them:
	PHP_EVAL_INCLINE($LUA_CFLAGS)
	PHP_EVAL_LIBLINE($LUA_LIBS, LUASANDBOX_SHARED_LIBADD)

	PHP_SUBST(LUASANDBOX_SHARED_LIBADD)
	PHP_NEW_EXTENSION(luasandbox, alloc.c data_conversion.c library.c luasandbox.c timer.c luasandbox_lstrlib.c, $ext_shared)
	PHP_ADD_MAKEFILE_FRAGMENT
fi
