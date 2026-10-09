--TEST--
LuaSandbox bit32 library availability vs actual Lua version (present 5.2/5.3 only)
--FILE--
<?php
$info = LuaSandbox::getVersionInfo();
preg_match('/(\d+)\.(\d+)/', $info['Lua'], $m);
$bit32Available = ($m[1] === '5' && (int)$m[2] >= 2);

$s = new LuaSandbox();
var_dump((string) $s->loadString('return type(bit32)')->call()[0] === ($bit32Available ? 'table' : 'nil'));
?>
--EXPECT--
bool(true)
