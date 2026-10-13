LuaSandbox is an extension for PHP 7 and 8 to allow safely running
untrusted Lua code from within PHP, which will generally be faster
than shelling out to a Lua binary and using inter-process communication.

This is a fork created for https://traditio.wiki.

What's new:
 - **Lua 5.1–5.4 support**: the source has been ported to build and run
   against any of Lua 5.1, 5.2, 5.3 or 5.4 (a compatibility shim,
   `luasandbox_lua_compat.h`, bridges the API differences). `bit32` is
   registered on Lua 5.2/5.3 (where it is native); `math.pow` and
   `math.log10` are available on all supported versions.
 - Added `luasandbox.ini_script` (scope `PHP_INI_SYSTEM`) directive to define a Lua script run every time a state is created; in that script available globals can be filtered, as well as additional libraries `require`'d via its `censor()` helper (see AGENTS.md).
 - Backported `table.move` for Lua 5.1-5.4: exposed unconditionally at registration with the source-size bound from upstream commit 38e5324. Unlike native Lua >=5.4 it returns a copy of the moved values rather than an integer index, so read results positionally from `call()`; see tests/table-move.phpt.

## Requirements

- PHP 7.2 or newer (tested against PHP 8.3, 8.4 and 8.5)
- Development headers for one of Lua 5.1, 5.2, 5.3 or 5.4
- `pkg-config` (used exclusively to locate Lua and derive compiler/linker flags)
- A C compiler toolchain (`build-essential` / `gcc`, `make`, `autoconf`)

## Building and installing

Standard phpize flow:

```bash
phpize
./configure
make
sudo make install
```

### `./configure` defaults explained

`./configure` with no arguments is equivalent to `--with-luasandbox=yes`:

- The Lua library is located **via pkg-config**. The module search order is
  `lua5.1`, then the Debian/FreeBSD-style (`lua-5.1`, `lua51`) and plain
  (`lua5.2` … `lua5.4`, `lua`) spellings — i.e. **5.1 wins if several Lua
  versions are installed side by side**. Anything from 5.1 to 5.4 (inclusive)
  is accepted; the detected module/version is printed by configure
  (`checking for Lua... lua5.4 5.4.8`).
- To pin a specific Lua version, pass the pkg-config module name:
  `./configure --with-luasandbox=lua5.4`
  (also accepts `lua-5.4` etc. — whatever `pkg-config --exists` knows).
- With several PHP builds installed, the build targets the PHP that owns the
  `phpize`/`php-config` in your `PATH`. Select another one explicitly:
  `./configure --with-php-config=/usr/bin/php-config8.3`
  (Debian/Ubuntu ship dotted `php-config8.3` names, Alpine ships `php-config83`).
- `./config.nice` re-runs configure with the last used flags — handy for
  rebuilding the same PHP × Lua combination.

No manual `LUA_CFLAGS`/`LUA_LIBS` are needed; configure derives them from
`pkg-config`.

### Debian / Ubuntu, PHP 8.3 + Lua 5.4 (from packages)

```bash
sudo apt-get install build-essential php-dev pkg-config liblua5.4-dev
phpize && ./configure --with-php-config=/usr/bin/php-config8.3 --with-luasandbox=lua5.4
make
sudo make install
sudo phpenmod -v 8.3 luasandbox           # enable the extension (CLI + FPM)
sudo service php8.3-fpm restart
```

Package names for the other Lua versions: `liblua5.1-0-dev`, `liblua5.2-dev`,
`liblua5.3-dev`. `php-dev` is the default-version meta package; install
`php8.3-dev` etc. for a specific SAPI set.

### Alpine Linux, PHP 8.3 + Lua 5.4 (from packages)

```bash
sudo apk add build-base pkgconfig php83-dev lua5.4-dev
phpize83 && ./configure --with-php-config=/usr/bin/php-config83 --with-luasandbox=lua5.4
make
sudo make install
# Alpine has no phpenmod; enable manually:
echo 'extension=luasandbox.so' | sudo tee /etc/php83/conf.d/luasandbox.ini
sudo rc-service php-fpm83 restart
```

Adjust `php83`/`php-config83`/`phpize83` to your PHP minor (`apk` names them
`php85`, `php-config85`, `phpize85`, …). Lua dev packages are
`lua5.1-dev`, `lua5.2-dev`, `lua5.3-dev`, `lua5.4-dev`.

### Verifying the build

```bash
php -m | grep luasandbox
php -r 'print_r(LuaSandbox::getVersionInfo());'   # LuaSandbox + Lua versions
```

## Running the tests

```bash
make test                                   # full .phpt suite
make TESTFILES="tests/call.phpt" test       # a single test
```

To build and test **every installed PHP × Lua combination** in one go:

```bash
bash scripts/test_all_combinations.sh              # full matrix
bash scripts/test_all_combinations.sh 83 lua5.4    # PHP 8.3 × Lua 5.4 only
DRY_RUN=1 bash scripts/test_all_combinations.sh    # just list what would run
```

Each combination is configured, compiled and tested into its own log under
`scripts/builds/` (e.g. `php83-lua5.4.log`).

For more details see <https://www.mediawiki.org/wiki/LuaSandbox> and
[AGENTS.md](AGENTS.md).
