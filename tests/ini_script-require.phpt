--TEST--
luasandbox.ini_script: require() available to load additional libraries (package library restored)
--INI--
luasandbox.ini_script="package.preload[\"hello\"]=function() return {greet=function() return \"hi\" end} end hello=require \"hello\" require_type=type(require) package_type=type(package)"
--FILE--
<?php
// Custom ini_script: require() works while the state is created, so
// additional libraries (lpeg, lrexlib, pure-Lua modules via package.preload)
// can be loaded for the sandbox.
$s = new LuaSandbox();
var_dump( $s->loadString( 'return hello.greet()' )->call()[0] );
var_dump( $s->loadString( 'return require_type, package_type' )->call() );
?>
--EXPECT--
string(2) "hi"
array(2) {
  [0]=>
  string(8) "function"
  [1]=>
  string(5) "table"
}
