--TEST--
LuaSandbox math.pow / math.log10 availability (removed from Lua as of 5.3, restored by LuaSandbox on all versions)
--FILE--
<?php
$s = new LuaSandbox();
var_dump((string) $s->loadString('return type(math.pow)')->call()[0]);
var_dump((string) $s->loadString('return type(math.log10)')->call()[0]);
var_dump($s->loadString('return math.pow(2, 10), math.log10(1000)')->call());
?>
--EXPECTF--
string(8) "function"
string(8) "function"
array(2) {
  [0]=>
  %A(1024)
  [1]=>
  %A(3)
}
