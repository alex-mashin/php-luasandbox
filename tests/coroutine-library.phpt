--TEST--
coroutine library registered (Lua 5.1 via luaopen_base, 5.2+ via luaopen_coroutine) and usable when the ini script allowlists it
--INI--
luasandbox.ini_script="cc=type(coroutine.create) cw=type(coroutine.wrap) cr=type(coroutine.resume) cy=type(coroutine.yield) cs=type(coroutine.status)"
--FILE--
<?php
// The extension registers luaopen_coroutine on Lua 5.2-5.4 (since 5.2 it is
// no longer a side effect of luaopen_base). The default ini script censor
// removes it again; a custom script may allowlist it (not security-reviewed
// upstream, allowlisting is the admin's decision).
$s = new LuaSandbox;
var_dump( $s->loadString(
	'local co = coroutine.wrap(function(v) return v * 2 end) return co(21), cc, cw, cr, cy, cs'
)->call() );
--EXPECT--
array(6) {
  [0]=>
  int(42)
  [1]=>
  string(8) "function"
  [2]=>
  string(8) "function"
  [3]=>
  string(8) "function"
  [4]=>
  string(8) "function"
  [5]=>
  string(8) "function"
}
