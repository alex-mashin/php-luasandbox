--TEST--
Passing a LuaSandboxFunction to a different sandbox is rejected (F7b guard)
--FILE--
<?php
$a = new LuaSandbox();
$b = new LuaSandbox();

$f = $a->loadString( 'return 42' );

// Happy path: same-sandbox passing must keep working.
var_dump( $a->loadString( 'local f = ...; return f()' )->call( $f ) );

// Cross-sandbox: the owning-sandbox guard must warn and the call must fail
// cleanly (false) instead of silently resolving the chunk index in the
// destination sandbox's registry (which could invoke a wrong function).
set_error_handler( function ( $errno, $errstr ) {
	echo "E_WARNING: $errstr\n";
	return true;
} );
var_dump( $b->loadString( 'local f = ...; return f()' )->call( $f ) );
restore_error_handler();
?>
--EXPECTF--
array(1) {
  [0]=>
  int(42)
}
E_WARNING: %sUnable to pass a LuaSandboxFunction to a different sandbox
E_WARNING: %sunable to convert argument 1 to a lua value
bool(false)
