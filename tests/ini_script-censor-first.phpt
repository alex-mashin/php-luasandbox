--TEST--
luasandbox.ini_script: censor-first pattern (censor before require; local require keeps working afterwards)
--INI--
luasandbox.ini_script="package.preload[\"hello\"]=function() return \"H\" end local _require=require local function censor (f,x) for k in pairs (f) do if not x[k] then f[k]=nil end end end censor (_G, {type=true, tostring=true}) after_censor=_require \"hello\" require_type=type(require) package_type=type(package) os_type=type(os)"
--FILE--
<?php
// The censor-first pattern for custom ini scripts: censor _G first, then
// keep loading libraries through a local `require` reference saved before
// the censor. Proves that:
//  - require() keeps working inside the ini script after the censor removed
//    the require/package globals (loading goes through the package table
//    itself, which the censor does not destroy),
//  - the sandbox is already safe while the script continues: require,
//    package and os are gone for untrusted code.
$s = new LuaSandbox;
var_dump( $s->loadString( 'return after_censor, require_type, package_type, os_type' )->call() );
--EXPECT--
array(4) {
  [0]=>
  string(1) "H"
  [1]=>
  string(3) "nil"
  [2]=>
  string(3) "nil"
  [3]=>
  string(3) "nil"
}
