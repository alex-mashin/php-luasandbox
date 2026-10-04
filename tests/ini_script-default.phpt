--TEST--
LuaSandbox ini_script default censor allowlist (allowed vs removed globals)
--FILE--
<?php
$s = new LuaSandbox();
var_dump( $s->loadString('return type(math)')->call()[0] );
var_dump( $s->loadString('return tostring(debug.getinfo)')->call()[0] );
var_dump( $s->loadString('return debug.traceback ~= nil')->call()[0] );
?>
--EXPECT--
string(5) "table"
string(3) "nil"
bool(true)
