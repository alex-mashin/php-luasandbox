/**
 * This file holds the library of functions which are written in C and exposed
 * to Lua code, and the code which manages registration of both the custom
 * library and the parts of the standard Lua library which we allow.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <math.h>

#include <limits.h>

#include "php.h"
#include "php_luasandbox.h"

#ifdef LUASANDBOX_NO_CLOCK
#include <time.h>
#endif

static int luasandbox_base_tostring(lua_State * L);
static int luasandbox_math_random(lua_State * L);
static int luasandbox_math_randomseed(lua_State * L);
static int luasandbox_base_pcall(lua_State * L);
static int luasandbox_base_xpcall(lua_State *L);
static int luasandbox_os_clock(lua_State * L);

static int luasandbox_base_pairs(lua_State *L);
static int luasandbox_base_ipairs(lua_State *L);
static int luasandbox_math_pow(lua_State * L);
static int luasandbox_math_log10(lua_State * L);
static int luasandbox_table_move_key(lua_State * L, int idx, lua_Integer f,
	lua_Integer e, lua_Integer * out);
static int luasandbox_table_move(lua_State * L);

ZEND_EXTERN_MODULE_GLOBALS(luasandbox);

/** {{{  luasandbox_lib_register
 */
void luasandbox_lib_register(lua_State * L)
{
	// Load the standard libraries that we need
	luasandbox_luaL_requiref(L, LUA_GNAME, luaopen_base);
	luasandbox_luaL_requiref(L, LUA_TABLIBNAME, luaopen_table);
	luasandbox_luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math);
	luasandbox_luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug);
	luasandbox_luaL_requiref(L, LUA_OSLIBNAME, luaopen_os);
	// package provides require(), which luasandbox.ini_script uses to load
	// additional libraries (lpeg, lrexlib, ...). Commit 0cdc58f (the Lua
	// 5.1-5.4 compatibility port) accidentally dropped it, breaking ini
	// scripts that call require(). It is for trusted ini-script use only:
	// the ini script is expected to censor the require global and the
	// package table before untrusted code runs, as the default script does
	// not allowlist them (and untrusted code must never be able to extend
	// package.path/cpath).
	luasandbox_luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package);
#if LUA_VERSION_NUM >= 502
	// Since Lua 5.2 coroutine lives in its own library and is no longer
	// registered as a side effect of luaopen_base; register it explicitly.
	// It is removed again by the default ini script censor; a custom script
	// may allowlist it (traditio.wiki does).
	luasandbox_luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine);
#endif
#if LUA_VERSION_NUM >= 502 && LUA_VERSION_NUM < 504
	// bit32 ships with Lua 5.2 and 5.3 only; luaL_openlibs() would normally
	// register it via luaopen_bit32, which we bypass above.
	luasandbox_luaL_requiref(L, LUA_BITLIBNAME, luaopen_bit32);
#endif

	// Install our own string library
	lua_pushcfunction(L, luasandbox_open_string);
	lua_call(L, 0, 0);

	// Install our own versions of tostring, pcall, xpcall
	lua_pushcfunction(L, luasandbox_base_tostring);
	lua_setglobal(L, "tostring");
	lua_pushcfunction(L, luasandbox_base_pcall);
	lua_setglobal(L, "pcall");
	lua_pushcfunction(L, luasandbox_base_xpcall);
	lua_setglobal(L, "xpcall");

	// Remove string.dump: may expose private data
	lua_getglobal(L, "string");
	lua_pushnil(L);
	lua_setfield(L, -2, "dump");
	lua_pop(L, 1);

	// Install our own versions of math.random and math.randomseed
	lua_getglobal(L, "math");
	lua_pushcfunction(L, luasandbox_math_random);
	lua_setfield(L, -2, "random");
	lua_pushcfunction(L, luasandbox_math_randomseed);
	lua_setfield(L, -2, "randomseed");
	lua_pop(L, 1);

	// Install our own version of os.clock(), which uses our high-resolution
	// usage timer
	lua_getglobal(L, "os");
	lua_pushcfunction(L, luasandbox_os_clock);
	lua_setfield(L, -2, "clock");
	// Install our own table.move, bounded by the source table's real size;
	// see the comment on luasandbox_allowed_table_members.
	lua_getglobal(L, "table");
	lua_pushcfunction(L, luasandbox_table_move);
	lua_setfield(L, -2, "move");
	lua_pop(L, 1);
	lua_pop(L, 1);

	// Install our own versions of pairs and ipairs to preserve __pairs and
	// __ipairs support across the Lua versions supported by LuaSandbox.
	lua_getglobal(L, "pairs");
	lua_setfield(L, LUA_REGISTRYINDEX, "luasandbox_old_pairs");
	lua_getglobal(L, "ipairs");
	lua_setfield(L, LUA_REGISTRYINDEX, "luasandbox_old_ipairs");
	lua_pushcfunction(L, luasandbox_base_pairs);
	lua_setglobal(L, "pairs");
	lua_pushcfunction(L, luasandbox_base_ipairs);
	lua_setglobal(L, "ipairs");

	// Restore math.pow and math.log10, removed from Lua as of 5.3, as their
	// Lua 5.1 selves: thin wrappers around the equivalent libm calls,
	// unchanged by any Lua version.
	lua_getglobal(L, "math");
	lua_pushcfunction(L, luasandbox_math_pow);
	lua_setfield(L, -2, "pow");
	lua_pushcfunction(L, luasandbox_math_log10);
	lua_setfield(L, -2, "log10");
	lua_pop(L, 1);
}
/* }}} */

/** {{{ luasandbox_lib_destroy_globals */
void luasandbox_lib_destroy_globals()
{

}
/* }}} */

/** {{{ luasandbox_base_tostring
 *
 * This is identical to luaB_tostring except that it does not call lua_topointer().
 */
static int luasandbox_base_tostring(lua_State * L)
{
	luaL_checkany(L, 1);
	if (luaL_callmeta(L, 1, "__tostring"))  /* is there a metafield? */
		return 1;  /* use its value */
	switch (lua_type(L, 1)) {
		case LUA_TNUMBER:
			lua_pushstring(L, lua_tostring(L, 1));
			break;
		case LUA_TSTRING:
			lua_pushvalue(L, 1);
			break;
		case LUA_TBOOLEAN:
			lua_pushstring(L, (lua_toboolean(L, 1) ? "true" : "false"));
			break;
		case LUA_TNIL:
			lua_pushliteral(L, "nil");
			break;
		default:
			lua_pushfstring(L, "%s", luaL_typename(L, 1));
			break;
	}
	return 1;
}
/* }}} */

/** {{{ luasandbox_math_random
 *
 * A math.random implementation that doesn't share state with PHP's rand()
 */
static int luasandbox_math_random(lua_State * L)
{
	php_luasandbox_obj * sandbox = luasandbox_get_php_obj(L);

#ifdef PHP_WIN32
	// MSVC does not provide rand_r(). Note that srand/rand still does not
	// share state with PHP's rand() because PHP's rand() is an alias for mt_rand()
	// (and does not use C srand/rand) as of PHP 7.1.
	int i = rand();
#else
	int i = rand_r(&sandbox->random_seed);
#endif

	if (i >= RAND_MAX) {
		i -= RAND_MAX;
	}
	lua_Number r = (lua_Number)i / (lua_Number)RAND_MAX;
	switch (lua_gettop(L)) {  /* check number of arguments */
		case 0: {  /* no arguments */
			lua_pushnumber(L, r);  /* Number between 0 and 1 */
			break;
		}
		case 1: {  /* only upper limit */
			int u = luaL_checkint(L, 1);
			luaL_argcheck(L, 1<=u, 1, "interval is empty");
			lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
			break;
		}
		case 2: {  /* lower and upper limits */
			int l = luaL_checkint(L, 1);
			int u = luaL_checkint(L, 2);
			luaL_argcheck(L, l<=u, 2, "interval is empty");
			lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
			break;
		}
		default: return luaL_error(L, "wrong number of arguments");
	}
	return 1;
}
/* }}} */

/** {{{ luasandbox_math_randomseed
 *
 * Set the seed for the custom math.random.
 */
static int luasandbox_math_randomseed(lua_State * L)
{
	php_luasandbox_obj * sandbox = luasandbox_get_php_obj(L);
	sandbox->random_seed = (unsigned int)luaL_checkint(L, 1);
#ifdef PHP_WIN32
	srand(sandbox->random_seed);
#endif
	return 0;
}
/* }}} */

/** {{{ luasandbox_lib_rethrow_fatal
 *
 * If the error on the top of the stack with the error return code given as a
 * parameter is a fatal, rethrow the error. If the error is rethrown, the
 * function does not return.
 */
static void luasandbox_lib_rethrow_fatal(lua_State * L, int status)
{
	switch (status) {
		case 0:
			// No error
			return;
		case LUA_ERRRUN:
			// A runtime error which we can rethrow in a normal way
			if (luasandbox_is_fatal(L, -1)) {
				lua_error(L);
			}
			break;
		case LUA_ERRMEM:
		case LUA_ERRERR:
			// Lua doesn't provide a public API for rethrowing these, so we
			// have to convert them to our own fatal error type
			if (!luasandbox_is_fatal(L, -1)) {
				luasandbox_wrap_fatal(L);
			}
			lua_error(L);
			break; // not reached
	}
}
/* }}} */

/** {{{ luasandbox_lib_error_wrapper
 *
 * Wrapper for the xpcall error function
 */
static int luasandbox_lib_error_wrapper(lua_State * L)
{
	int status;
	luaL_checkany(L, 1);

	// This function is only called from luaG_errormsg(), which will later
	// unconditionally set the status code to LUA_ERRRUN, so we can assume
	// that the error type is equivalent to LUA_ERRRUN.
	if (luasandbox_is_fatal(L, 1)) {
		// Just return to whatever called lua_pcall(), don't call the user
		// function
		return lua_gettop(L);
	}

	// Put the user error function at the bottom of the stack
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, 1);
	// Call it, passing through the arguments to this function
	status = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);
	if (status != 0) {
		luasandbox_lib_rethrow_fatal(L, LUA_ERRERR);
	}
	return lua_gettop(L);
}
/* }}} */

/** {{{ luasandbox_base_pcall
 *
 * This is our implementation of the Lua function pcall(). It allows Lua code
 * to handle its own errors, but forces internal errors to be rethrown.
 */
static int luasandbox_base_pcall(lua_State * L)
{
	int status;
	luaL_checkany(L, 1);
	status = lua_pcall(L, lua_gettop(L) - 1, LUA_MULTRET, 0);
	luasandbox_lib_rethrow_fatal(L, status);
	lua_pushboolean(L, (status == 0));
	lua_insert(L, 1);
	return lua_gettop(L);  // return status + all results
}
/* }}} */

/** {{{ luasandbox_base_xpcall
 *
 * This is our implementation of the Lua function xpcall(). It allows Lua code
 * to handle its own errors, but forces internal errors to be rethrown.
 */
static int luasandbox_base_xpcall (lua_State *L)
{
	int status;
	luaL_checkany(L, 2);
	lua_settop(L, 2);

	// We wrap the error function in a C closure. The error function already
	// happens to be at the top of the stack, so we don't need to push it before
	// calling lua_pushcfunction to make it an upvalue
	lua_pushcclosure(L, luasandbox_lib_error_wrapper, 1);
	lua_insert(L, 1);  // put error function under function to be called

	status = lua_pcall(L, 0, LUA_MULTRET, 1);
	luasandbox_lib_rethrow_fatal(L, status);
	lua_pushboolean(L, (status == 0));
	lua_replace(L, 1);
	return lua_gettop(L);  // return status + all results
}
/* }}} */

/** {{{ luasandbox_os_clock
 *
 * Implementation of os.clock() which uses our high-resolution usage timer,
 * if available, to provide an accurate measure of Lua CPU usage since a
 * particular LuaSandbox object was created.
 */
static int luasandbox_os_clock(lua_State * L)
{
	double clk;

#ifdef LUASANDBOX_NO_CLOCK
	clk = ((double)clock())/(double)CLOCKS_PER_SEC;
#else
	struct timespec ts;
	php_luasandbox_obj * sandbox = luasandbox_get_php_obj(L);
	luasandbox_timer_get_usage(&sandbox->timer, &ts);
	clk = ts.tv_sec + 1e-9 * ts.tv_nsec;
#endif

	// Reduce precision to 20μs to mitigate timing attacks
	clk = round(clk * 50000) / 50000;

	lua_pushnumber(L, (lua_Number)clk);
	return 1;
}

/* }}} */

/** {{{ luasandbox_math_pow
 *
 * Lua 5.1's math.pow, removed from Lua as of 5.3 in favour of the ^ operator.
 */
static int luasandbox_math_pow(lua_State * L)
{
	lua_pushnumber(L, pow(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
	return 1;
}
/* }}} */

/** {{{ luasandbox_math_log10
 *
 * Lua 5.1's math.log10, removed from Lua as of 5.3 in favour of
 * math.log(x, 10).
 */
static int luasandbox_math_log10(lua_State * L)
{
	lua_pushnumber(L, log10(luaL_checknumber(L, 1)));
	return 1;
}
/* }}} */

/** {{{ luasandbox_base_pairs
 *
 * This is our implementation of the Lua function pairs(). It allows the Lua
 * 5.2 __pairs metamethod to override the standard behavior.
 */
static int luasandbox_base_pairs (lua_State *L)
{
	if (!luaL_getmetafield(L, 1, "__pairs")) {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, LUA_REGISTRYINDEX, "luasandbox_old_pairs");
	}
	lua_pushvalue(L, 1);
	lua_call(L, 1, 3);
	return 3;
}
/* }}} */

/** {{{ luasandbox_base_ipairs
 *
 * This is our implementation of the Lua function ipairs(). It allows the Lua
 * 5.2 __ipairs metamethod to override the standard behavior.
 */
static int luasandbox_base_ipairs (lua_State *L)
{
	if (!luaL_getmetafield(L, 1, "__ipairs")) {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, LUA_REGISTRYINDEX, "luasandbox_old_ipairs");
	}
	lua_pushvalue(L, 1);
	lua_call(L, 1, 3);
	return 3;
}
/* }}} */
// Threshold below which luasandbox_table_move() copies the [f, e] range
// directly instead of walking the whole source table with lua_next(): a
// range this short costs only a constant number of raw accesses regardless
// of e - f, so it doesn't need the lua_next() walk's protection, and taking
// this path keeps small moves on large tables as cheap as the native
// table.move rather than paying for a full table scan every time.
#define LUASANDBOX_TABLE_MOVE_SMALL_RANGE 10

/** {{{ luasandbox_table_move_key
 *
 * If the value at the given stack index is an integer-valued key in
 * [f, e], write it to *out and return 1; otherwise return 0.
 */
static int luasandbox_table_move_key(lua_State * L, int idx, lua_Integer f,
	lua_Integer e, lua_Integer * out)
{
	if (lua_type(L, idx) == LUA_TNUMBER) {
		lua_Number nk = lua_tonumber(L, idx);
		lua_Integer ik = (lua_Integer)nk;
		if ((lua_Number)ik == nk && ik >= f && ik <= e) {
			*out = ik;
			return 1;
		}
	}
	return 0;
}
/* }}} */

/** {{{ luasandbox_table_move
 *
 * A table.move which, unlike the native one, is safe to expose. For a range
 * of at least LUASANDBOX_TABLE_MOVE_SMALL_RANGE elements, it walks the
 * source table's actual entries with lua_next() instead of the requested
 * [f, e] range, so its cost is bounded by the table's real size rather than
 * by e - f, which the caller fully controls (table.move(t, 1, 2^40, 1, {})
 * would otherwise iterate 2^40 times without LuaSandbox's instruction count
 * hook, which only fires on Lua bytecode, ever getting a chance to run).
 * Shorter ranges are copied directly instead, since a constant-bounded
 * range can't be abused that way and it keeps small moves on large tables
 * as cheap as the native table.move.
 *
 * Unlike the native table.move, this requires both the source and
 * destination to be real tables: it doesn't support the __index/__newindex
 * metamethod fallback the native version does, matching every other
 * function in this file (foreach, foreachi, maxn, unpack, ...).
 */
static int luasandbox_table_move(lua_State * L)
{
	lua_Integer f = luaL_checkinteger(L, 2);
	lua_Integer e = luaL_checkinteger(L, 3);
	lua_Integer t = luaL_checkinteger(L, 4);
	int tt = lua_isnoneornil(L, 5) ? 1 : 5;
	int tmpidx;
	lua_Integer ik;

	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, tt, LUA_TTABLE);

	if (e < f) {
		// Empty range: nothing to move.
		lua_pushvalue(L, tt);
		return 1;
	}

	// Overflow checks ported from Lua 5.4's ltablib.c:tmove().
	luaL_argcheck(L, f > 0 || e < LUA_MAXINTEGER + f, 3,
		"too many elements to move");
	luaL_argcheck(L, t <= LUA_MAXINTEGER - (e - f + 1) + 1, 4,
		"destination wrap around");

	if (e - f < LUASANDBOX_TABLE_MOVE_SMALL_RANGE) {
		// Short range: copy it directly, choosing the iteration order that
		// keeps an overlapping move correct, exactly as Lua's own tmove()
		// does for the native table.move.
		lua_Integer i;
		if (!lua_rawequal(L, 1, tt) || t > e || t <= f) {
			for (i = 0; i <= e - f; i++) {
				lua_rawgeti(L, 1, f + i);
				lua_rawseti(L, tt, t + i);
			}
		} else {
			for (i = e - f; i >= 0; i--) {
				lua_rawgeti(L, 1, f + i);
				lua_rawseti(L, tt, t + i);
			}
		}
		lua_pushvalue(L, tt);
		return 1;
	}

	if (!lua_rawequal(L, 1, tt)) {
		// Different tables can never overlap, so it's safe to write the
		// destination directly while walking the source.
		lua_pushnil(L);  // first key
		while (lua_next(L, 1)) {
			if (luasandbox_table_move_key(L, -2, f, e, &ik)) {
				lua_pushvalue(L, -1);  // dup value
				lua_rawseti(L, tt, t + (ik - f));
			}
			lua_pop(L, 1);  // remove value, leave key for lua_next
		}
		lua_pushvalue(L, tt);
		return 1;
	}

	// Same table: the source and destination ranges may overlap, and
	// mutating a table while lua_next() is traversing it is undefined
	// behaviour, so collect the matching entries into a fresh table first
	// and only write them into the source/destination once that traversal
	// has finished. That also sidesteps needing to replicate the ordering
	// (ascending vs descending) the native implementation uses to make an
	// overlapping copy safe: nothing here reads from table 1 any more once
	// the writes start.
	lua_newtable(L);
	tmpidx = lua_gettop(L);
	lua_pushnil(L);  // first key
	while (lua_next(L, 1)) {
		if (luasandbox_table_move_key(L, -2, f, e, &ik)) {
			lua_pushinteger(L, ik);
			lua_pushvalue(L, -2);  // dup value
			lua_rawset(L, tmpidx);  // tmp[ik] = value
		}
		lua_pop(L, 1);  // remove value, leave key for lua_next
	}
	lua_pushnil(L);  // first key
	while (lua_next(L, tmpidx)) {
		ik = lua_tointeger(L, -2);
		lua_rawseti(L, tt, t + (ik - f));  // pops value, leaves key
	}
	lua_pushvalue(L, tt);
	return 1;
}
/* }}} */

