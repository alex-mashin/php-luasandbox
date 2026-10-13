--TEST--
LuaSandbox table.move backport: exposed on all Lua versions; correct move semantics incl. overlap ordering and empty range
--FILE--
<?php
$s = new LuaSandbox();
echo "table.move exposed as function: "; var_dump( (string) $s->loadString('return type(table.move)')->call()[0] );

// Non-overlapping forward write to distinct indices; source stays size 3.
$r1 = new LuaSandbox(); echo "non-overlapping move: "; echo var_export($r1->loadString('local src = { 10, 20, 30 }; table.move( src, 1, 2, 8 ); return #src, src[8], src[9]')->call(), 1) . "\n";

// Overlapping small-range (<10) copy into earlier indices stays correct.
$r2 = new LuaSandbox(); echo "overlapping small-range move: "; echo var_export($r2->loadString('local src = { 0, 0, 0, 0, 5, 6, 7 }; table.move( src, 5, 7, 1 ); return #src, src[1], src[2], src[3]')->call(), 1) . "\n";

// Empty range (e < f) is a no-op; source returned unchanged.
$r3 = new LuaSandbox(); echo "empty-range keeps original size/content: "; echo var_export($r3->loadString('local t = { 42 }; table.move( t, 10, 5, 3 ); return #t, t[1]')->call(), 1) . "\n";

// Error handling: a missing destination index must raise a catchable LuaSandboxError (no crash).
$e = new LuaSandbox(); try { $e->loadString('return table.move( {}, 1, math.maxinteger and -math.mininteger or 0 )')->call(); } catch ( Exception $ex ) { echo "Exception: " . $ex->getMessage() . "\n"; }

--EXPECT--
table.move exposed as function: string(8) "function"
non-overlapping move: array (
  0 => 3,
  1 => 10,
  2 => 20,
)
overlapping small-range move: array (
  0 => 7,
  1 => 5,
  2 => 6,
  3 => 7,
)
empty-range keeps original size/content: array (
  0 => 1,
  1 => 42,
)
Exception: [string ""]:1: bad argument #4 to 'move' (number expected, got no value)
