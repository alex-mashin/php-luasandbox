--TEST--
LuaSandbox deprecated math.pow / math.log10 availability vs actual Lua version (removed by default from 5.3)
--FILE--
<?php
$info = LuaSandbox::getVersionInfo();
preg_match('/(\d+)\.(\d+)/', $info['Lua'], $m);
$deprecatedRemoved = ($m[1] === '5' && (int)$m[2] >= 3);

$s = new LuaSandbox();
var_dump((string) $s->loadString('return type(math.pow)')->call()[0] === ($deprecatedRemoved ? 'nil' : 'function'));
var_dump((string) $s->loadString('return type(math.log10)')->call()[0] === ($deprecatedRemoved ? 'nil' : 'function'));
?>
--EXPECT--
bool(true)
bool(true)
