--TEST--
LuaSandbox::wrapPhpFunction() with no arguments must not crash (F1 regression)
--FILE--
<?php
// PHP >= 8.0 throws ArgumentCountError from the declared arginfo; PHP 7.2-7.4
// hits the C arg-parse failure path, which used to fall through to lua_cpcall
// with an uninitialized zval pointer (UB/crash) and now returns false.
set_error_handler( function () { /* swallow the PHP 7 ZPP warning */ } );
$s = new LuaSandbox();
$rejected = false;
try {
	$rejected = ( $s->wrapPhpFunction() === false );
} catch ( ArgumentCountError $e ) {
	$rejected = true;
}
restore_error_handler();
echo $rejected ? "rejected\n" : "NOT rejected\n";
echo "still running (no crash)\n";
?>
--EXPECT--
rejected
still running (no crash)
