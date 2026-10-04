# AGENTS

Documentation for developers working on LuaSandbox, a PHP extension (C) that runs untrusted Lua 5.1 code safely from within PHP. This is a fork originally created for [traditio.wiki](https://traditio.wiki).

## Prerequisites / dependencies

This extension builds and runs either Debian-based distros (apt/dpkg) or Alpine Linux (`apk`). Install PHP development headers that provide `phpize`, plus Lua 5.1 dev bindings, then a C compiler:

- **Debian / Ubuntu**: `php-dev` (PHP development headers, providing `phpize`) and `liblua5.1-0-dev`.
- **Alpine Linux** (`apk`): the PHP development toolchain from apk's PHP package(s) plus Lua 5.1 bindings via `lua5.1-dev`. On Alpine, install tools are versioned by major (e.g. `/usr/bin/phpize<ver>`, `/usr/bin/php-config<ver>`), so pass the exact config path to `configure` as below.
- PHP 7.2+ (tested on PHP 8.x) built with FPM or CLI.

## Building the extension

Standard PECL build flow:

```bash
sudo apt-get install php-dev liblua5.1-0-dev   # system dependencies
phpize && ./configure && make                  # compile
sudo make install                              # install the .so into PHP's ext dir
sudo phpenmod luasandbox                        # enable it (FPM)
sudo service php8.x-fpm restart                 # reload the web server
```

### Building on Alpine Linux (`apk`)

Alpine installs Lua headers in the default include path but does not link them automatically, so pass the Lua flags to `configure` and point it at your PHP config. Using apk package names:

```bash
sudo apk add --no-cache php-dev liblua5.1-dev build-base   # system dependencies
phpize && ./configure LUA_CFLAGS="-I/usr/include" LUA_LIBS="-llua -lm" && make
sudo make install                                          # install the .so into PHP's ext dir
```

If your `phpize`/`php-config` are versioned (e.g. `phpize83`, `php-config83`), pass them explicitly:

```bash
./configure --with-php-config=/usr/bin/php-config<ver> LUA_CFLAGS="-I/usr/include" LUA_LIBS="-llua -lm" && make
```

On Windows, build against `config.w32` instead of `config.m4`.

## Running tests

Tests are written in the standard PHP `.phpt` format and run through PHP's test runner:

```bash
make test        # runs all .phpt files under tests/ (alias: make TESTFILES="tests/x.phpt")
```

- To run a single test: `make TESTFILES="tests/call.phpt"`.
- Add `-V` for verbose output (`make -V`) or `-k` to keep `.out`/`.diff` files.
- Each `.phpt` has a `--TEST--`, `--FILE--` (PHP source), and `--EXPECT--` / `--EXPECTED--` block comparing actual stdout against expected output.

## Linters

There is no dedicated linter or formatter configured in this repo (no ESLint/PHP-CS-Fixer/etc.). The C code follows the existing style conventions below; check new code manually against them before committing.

## Coding conventions

- **Language**: Extension implementation and Lua bindings are written in C (`*.c`, `*.h`). PHP stubs live in `stubs/`.
- **Indentation**: Tabs, not spaces.
- **Operators / parentheses**: Spaces around binary operators (e.g. `$i + 1`), no space inside function-call parens or after a comma that precedes an argument list opening brace is left as-is per existing files — match the surrounding style in each file (`php_luasandbox.h`, `luasandbox.c`, `data_conversion.c`, etc.).
- **Braces**: K&R / Allman mixed as already used throughout; follow the convention of the specific function/file you are editing.
- **Naming**: Prefix C symbols with `lua_sandbox_` or `LuaSandbox` consistently (e.g. `php_luasandbox.h`). Lua-side strings and identifiers use snake_case where PHP-facing, matching existing APIs like `loadString`, `wrapPhpFunction`, `registerLibrary`.
- **Safety-first**: This extension runs untrusted code under time/memory limits — never remove or weaken sandboxing guards, limit checks, or the reference-counting / memory management in `alloc.c` and `data_conversion.c` without care.
- **Tests for behavior changes**: Any change to observable Lua↔PHP data conversion must add/update a `.phpt` test with an accurate `--EXPECT--`.

## Configuring `lua_sandbox.ini_script`

The PHP directive `lua_sandbox.ini_script` (scope `PHP_INI_SYSTEM`) sets the Lua script run every time a state is created. It ships and can curate a **default allowlist** via a `censor()` helper that removes any global not explicitly allowed; this governs which globals/library functions untrusted code sees at runtime.

- Default behavior: all globals except an explicit allowlist are removed — e.g. `debug.getinfo` is censored (`nil`) while the allowed `debug.traceback` stays (see `lua_sandbox.ini`, default block, and docbook `docbook/lua_sandbox/ini.xml`). Prove it with `tests/ini_script-default.phpt`.
- Customizing: set `lua_sandbox,ini_script = "..."` in php.ini. The whole value must be double-quoted; inner literal quotes are escaped as `\"`. Inside, you may filter globals or `require` additional libraries (see the example at the top of `lua_sandb ox.ini`).
  - To exercise a custom script from `.phpt`, set it via a test's `--INI--` section. The extension auto-loads under `make test`; run with `make TESTFILES="tests/ini_script-custom.phpt" test`.
- Environment note: empirically, standard-library globals are re-injected after the ini script runs on this build, so custom allowlists cannot disable standard library functions — use the directive to *enable* additional libraries / grant access rather than remove stdlib (`tests/ini_script-custom.phpt`).

## Layout

- Extension sources: root (`*.c`, `*.h`, `config.m4`).
- Documentation (php.net docbook): `docbook/`.
- PHP stubs / type info: `stubs/`.
- Tests: `tests/*.phpt`.
