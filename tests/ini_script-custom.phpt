--TEST--
LuaSandbox ini_script configurable via php.ini; sandbox and standard libs stay usable
--FILE--
<?php
$s = new LuaSandbox();
var_dump( $s->loadString('return type(math)')->call()[0] );
var_dump( $s->loadString('local function f() return "ok" end; return f()')->call()[0] );
var_dump( $s->loadString('return _VERSION')->call()[0] );
?>
--INI--
luasandbox.ini_script = "_VERSION=nil"
--EXPECT--
string(5) "table"
string(2) "ok"
NULL
