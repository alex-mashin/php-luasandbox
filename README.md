LuaSandbox is an extension for PHP 7 and 8 to allow safely running
untrusted Lua 5.1 code from within PHP, which will generally be faster
than shelling out to a Lua binary and using inter-process communication.

This is a fork created for https://traditio.wiki.

What's new:
 - Added luasandbox.ini_script to php.ini to define a Lua script that runs every time a Lua state is created. In that script available globals can be filtered

For more details see <https://www.mediawiki.org/wiki/LuaSandbox>.
