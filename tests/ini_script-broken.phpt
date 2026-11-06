--TEST--
LuaSandbox ini_script with a syntax error: catchable E_WARNING, state stays usable
--INI--
luasandbox.ini_script = "this is not valid lua("
--FILE--
<?php
set_error_handler( function( $errno, $errstr ) {
	// php_error_docref() prefixes the active function name; anchor on the
	// fixed marker so the test does not depend on the caller's name.
	echo "E_WARNING caught: " . strtok( $errstr, "\n" ) . "\n";
	echo "has marker: " . var_export( strpos( $errstr, 'luasandbox.ini_script failed:' ) !== false, true ) . "\n";
}, E_ALL );

$s = new LuaSandbox();
var_dump( $s->loadString( 'return type(math)' )->call()[0] );
var_dump( $s->loadString( 'local function f() return "ok" end; return f()' )->call()[0] );
?>
--EXPECTF--
E_WARNING caught: %sluasandbox.ini_script failed: %s
has marker: true
string(5) "table"
string(2) "ok"
