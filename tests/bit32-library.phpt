--TEST--
LuaSandbox bit32 library availability vs actual Lua version (present natively in 5.2/5.3 only, absent in 5.1/5.4)
--FILE--
<?php
$info = LuaSandbox::getVersionInfo();
preg_match('/(\d+)\.(\d+)/', $info['Lua'], $m);
$minor = (int)$m[2];
$bit32Available = ($m[1] === '5' && ($minor === 2 || $minor === 3));

$s = new LuaSandbox();
var_dump((string) $s->loadString('return type(bit32)')->call()[0] === ($bit32Available ? 'table' : 'nil'));
?>
--EXPECT--
bool(true)
