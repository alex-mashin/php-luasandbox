--TEST--
LuaSandbox ini_script configurable via php.ini; sandbox and standard libs stay usable
--FILE--
<?php
$s = new LuaSandbox();
var_dump( $s->loadString('return math.floor(2.7)')->call()[0] );
var_dump( $s->loadString('local function f() return "ok" end; return f()')->call()[0] );
?>
--INI--
lua_sandb ox_ini_script = "censor(_G, {_VERSION=true})"
--EXPECT--
int(2)
string(2) "ok"
