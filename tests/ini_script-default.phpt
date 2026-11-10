--TEST--
LuaSandbox ini_script default censor allowlist (allowed vs removed globals)
--FILE--
<?php
$s = new LuaSandbox();
var_dump( $s->loadString('return type(math)')->call()[0] );
var_dump( $s->loadString('return tostring(debug.getinfo)')->call()[0] );
var_dump( $s->loadString('return debug.traceback ~= nil')->call()[0] );
// The default censor script does not allowlist require/package,
// so untrusted code must not see them even though the C code
// registers the package library (for use by ini scripts).
// Same for coroutine: registered by the extension (explicitly on
// Lua 5.2+), but removed by the default censor.
var_dump( $s->loadString('return type(require), type(package), type(coroutine)')->call() );
?>
--EXPECT--
string(5) "table"
string(3) "nil"
bool(true)
array(3) {
  [0]=>
  string(3) "nil"
  [1]=>
  string(3) "nil"
  [2]=>
  string(3) "nil"
}
