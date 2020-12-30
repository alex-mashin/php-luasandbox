LuaSandbox is an extension for PHP 7 and 8 to allow safely running
untrusted Lua 5.1 code from within PHP, which will generally be faster
than shelling out to a Lua binary and using inter-process communication.

This is a fork created for https://traditio.wiki.

What's new:
 - Added `luasandbox.ini_script` to `php.ini` to define a Lua script that runs every time a Lua state is created. In that script available globals can be filtered, as well as additional libraries `require`'d.

To build and enable, assuming that Debian-based Linux distro, PHP 8.0 and FPM are used:
`sudo apt-get install php-dev liblua5.1-0-dev && phpize && ./configure && sudo make install && sudo phpenmod luasandbox && sudo service php8.0-fpm restart`.

For more details see <https://www.mediawiki.org/wiki/LuaSandbox>.
