# AGENTS

Documentation for developers working on LuaSandbox, a PHP extension (C) that runs untrusted Lua code safely from within PHP. This is a fork originally created for [traditio.wiki](https://traditio.wiki).

## Prerequisites / dependencies

The extension supports **Lua 5.1, 5.2, 5.3 and 5.4** (source-level compatibility via `luasandbox_lua_compat.h`) and builds on Debian-based distros (apt/dpkg) and Alpine Linux (`apk`). Lua is located **exclusively via pkg-config** — install the dev bindings for whichever version you target, plus a PHP dev toolchain providing `phpize`/`php-config`:

- **Debian / Ubuntu**: `build-essential php-dev pkg-config` plus one of `liblua5.1-0-dev`, `liblua5.2-dev`, `liblua5.3-dev`, `liblua5.4-dev`. For a non-default PHP, also the matching `php8.x-dev`.
- **Alpine Linux** (`apk`): `build-base pkgconfig`, the PHP dev package for your version (`php83-dev`, `php84-dev`, `php85-dev`, …) plus one of `lua5.1-dev`, `lua5.2-dev`, `lua5.3-dev`, `lua5.4-dev`. Alpine tools are version-suffixed (`phpize83`, `php-config83`), Debian/Ubuntu dotted (`phpize8.3`, `php-config8.3`).
- PHP 7.2+ (tested against PHP 8.3, 8.4, 8.5) built with FPM or CLI.

## Building the extension

Standard phpize flow (defaults explained below):

```bash
phpize
./configure
make
sudo make install
```

- `./configure` with no arguments equals `--with-luasandbox=yes`: pkg-config probes modules in the order `lua5.1 lua-5.1 lua51 lua5.2 … lua5.4 … lua` and takes the **first match** — so 5.1 wins when several Lua versions are installed. Accepted range: 5.1–5.4; the detected module/version is printed (`checking for Lua... lua5.4 5.4.8`).
- Pin a Lua version by module name: `./configure --with-luasandbox=lua5.4` (any pkg-config spelling works: `lua5.4`, `lua-5.4`, …).
- Multi-PHP machines: the build targets the `phpize`/`php-config` in `PATH`; select another with `./configure --with-php-config=/usr/bin/php-config8.3` (dotted names on Debian/Ubuntu, `php-config83` on Alpine).
- No manual `LUA_CFLAGS`/`LUA_LIBS` are needed — configure derives include/lib flags from pkg-config.
- `./config.nice` re-runs configure with the previous flags.

Enable/reload after install (Debian/Ubuntu):

```bash
sudo phpenmod -v 8.3 luasandbox        # enable for CLI+FPM (omit -v for default PHP)
sudo service php8.3-fpm restart
```

### Building on Alpine Linux (`apk`)

```bash
sudo apk add --no-cache build-base pkgconfig php83-dev lua5.4-dev
phpize83 && ./configure --with-php-config=/usr/bin/php-config83 --with-luasandbox=lua5.4 && make
sudo make install
# Alpine has no phpenmod; enable manually:
echo 'extension=luasandbox.so' | sudo tee /etc/php83/conf.d/luasandbox.ini
sudo rc-service php-fpm83 restart
```

(Older instructions recommending `LUA_CFLAGS="-I/usr/include" LUA_LIBS="-llua -lm"` are obsolete: `config.m4` uses `PKG_CHECK_MODULES`, so those variables are no longer consumed.)

There is no `config.w32` in this fork — Windows is not supported out of the box (the build system is `config.m4` + phpize).

## Running tests

Tests are written in the standard PHP `.phpt` format and run through PHP's test runner:

```bash
make test        # runs all .phpt files under tests/
```

- To run a single test: `make TESTFILES="tests/call.phpt"`.
- Add `-V` for verbose output (`make -V`) or `-k` to keep `.out`/`.diff` files.
- Each `.phpt` has a `--TEST--`, `--FILE--` (PHP source), and `--EXPECT--` / `--EXPECTF--` block comparing actual stdout against expected output.

## Building & testing across PHP × Lua versions

`scripts/test_all_combinations.sh` auto-discovers installed PHP runtimes (via `php-config`, bare or version-suffixed, Alpine and Debian naming both handled) and Lua modules (via pkg-config), then builds + runs every combination via `make test`, logging each to `scripts/builds/` (e.g. `php83-lua5.4.log`). Validated matrix: PHP 8.3–8.5 × Lua 5.1–5.4 — all 12 combos pass.

```bash
DRY_RUN=1 bash scripts/test_all_combinations.sh      # discover only, no build
bash scripts/test_all_combinations.sh                # full matrix
bash scripts/test_all_combinations.sh 83 lua5.4      # PHP 8.3 × Lua 5.4 only
bash scripts/test_all_combinations.sh 84 85 lua5.1   # PHP 8.4+8.5 × Lua 5.1
```

Filter arguments: PHP keys are the major/minor suffixes of the discovered `php-config` (`83`, `84`, `85`); Lua filters accept `lua5.4` or `5.4` (a bare `lua` argument is ignored). Non-zero exit means at least one combo failed or errored; per-combo logs under `scripts/builds/` are appended across runs — delete them for a clean view.

**Gotcha (already fixed):** chained parameter-expansion assignments such as `${var#*Passes:}` must stay **unquoted** when placed on a physical line right after a `$(grep ... "$logf" ...)`. Wrapping them in `"${...}"` makes bash's flat quote-matcher leave one double-quote unmatched and fail with "unexpected EOF while looking for matching \"`. Keep the command substitution quoted (for safety) but write parameter-expansion chains unquoted.

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

## Configuring `luasandbox.ini_script`

The PHP directive `luasandbox.ini_script` (scope `PHP_INI_SYSTEM`) sets the Lua script run every time a state is created. It ships and can curate a **default allowlist** via a `censor()` helper that removes any global not explicitly allowed; this governs which globals/library functions untrusted code sees at runtime.

- Default behavior: all globals except an explicit allowlist are removed — e.g. `debug.getinfo` is censored (`nil`) while the allowed `debug.traceback` stays (see `luasandbox.ini`, default block, and docbook `docbook/luasandbox/ini.xml`). Prove it with `tests/ini_script-default.phpt`.
- Customizing: set `luasandbox.ini_script = "..."` in php.ini. The whole value must be double-quoted; inner literal quotes are escaped as `\"`. Inside, you may filter globals or `require` additional libraries (see the example at the top of `luasandbox.ini`).
  - To exercise a custom script from `.phpt`, set it via a test's `--INI--` section. The extension auto-loads under `make test`; run with `make TESTFILES="tests/ini_script-custom.phpt" test`.
- Ordering: the standard library is registered *before* the ini script runs (`luasandbox.c:419` vs `:437`), so a custom script can censor or replace stdlib globals too — nothing is re-injected afterwards. `tests/ini_script-custom.phpt` runs `_VERSION=nil` and proves the script executes (`return _VERSION` → `NULL`) while the state stays usable.
## Backported `table.move` (Lua 5.1–5.4)

- Exposed unconditionally in lib registration for every supported Lua version via forward decls (`lua.sandbox_table_move`, `lua.sandbox_table_move_key`) and an install block: `lua_pushcfunction(L, lua.sandbox_table_move); lua_setfield(L, -2, "move")`. The source-size bound from upstream commit `38e5324` is applied through a shimmed `LUASANDBOX_TABLE_MOVE_SMALL_RANGE 10`; the self-contained fallback also covers strict-<5.3 Lua headers that omit `LUA_MAXINTEGER`. See library.c:39-43, :96-100 and :409+.
- Return contract differs from native Lua >=5.4: it returns a copy of the moved values (the destination indices), not an integer index — read results positionally from `call()`, e.g. `$sandbox->loadString('return #t, t[8]')->call()` after moving into `t`. Non-overlapping forward writes and overlapping small-range (`<10`) copies both preserve correct ordering; empty ranges (e<f) are no-ops that leave size/content unchanged.
- Out-of-bounds indices raise a catchable LuaSandboxError with messages like "too many elements to move" / "destination wrap around" — the extension must not crash. Verify exposure + semantics incl. overlap ordering and empty range via `make TESTFILES="tests/table-move.phpt" test`.

## Layout

- Extension sources: root (`*.c`, `*.h`, `config.m4`).
- Documentation (php.net docbook): `docbook/`.
- PHP stubs / type info: `stubs/`.
- Tests: `tests/*.phpt`.
- Multi-version script: `scripts/test_all_combinations.sh` discovers installed PHP runtimes and Lua modules, then builds + runs every combination via `make test`, logging each to `scripts/builds/`; use `DRY_RUN=1 bash scripts/test_all_combinations.sh` to only list discovered versions. See "Building & testing across PHP × Lua versions" below.
- Upstream: this is a fork of [wikimedia/mediawiki-php-luasandbox](https://github.com/wikimedia/mediawiki-php-luasandbox). The `table.move` backport (commit `38e5324`) originates there; see the "Backported table.move" section.

## Known issues

Known defects found during the static review (full details with line references in `REVIEW.md`):

- ~~**`wrapPhpFunction` missing return**~~ **FIXED**: `RETURN_FALSE;` now returns on arg-parse failure (`luasandbox.c`), instead of calling `lua_cpcall` with an uninitialized zval (UB/crash on PHP 7.2–7.4).
- ~~**`active_count` accounting drifts**~~ **FIXED**: counter now balanced (incremented in `luasandbox_new` only); the duplicate increment in `luasandbox_newstate` was removed; ini-script failure at state creation now raises a catchable E_WARNING with the Lua error message instead of an uncatchable `E_ERROR` via `luasandbox_panic()`.
- ~~**Profiler hook leaks Lua stack slots**~~ **FIXED**: the hook snapshots/restores `lua_gettop` around its work and `luasandbox_timer_get_cfunction_name` uses a caller-owned stack buffer (ZTS-safe) and pops the upvalue on every path.
- ~~**Fail-open CPU limit**~~ **FIXED**: `luasandbox_call_lua` now fails closed when a CPU limit is set — throws `LuaSandboxTimeoutError` ("Unable to start limit timer") rather than running unbounded; `timer_settime()` failures during arming are detected and treated as timer-start failure.
- ~~**Timer internals**~~ **FIXED**: `hash_for_id` now uses unsigned arithmetic (no signed-overflow UB for large timer ids); `luasandbox_timer_stop_one` no longer frees the timer when `sem_wait` fails with non-`EINTR` — it leaks it instead (bounded, near-unreachable) to avoid a use-after-free with a live `SIGEV_THREAD` handler.
- ~~**Allocator hidden coupling**~~ **FIXED**: `luasandbox_update_gc_pause` explicitly skips when `obj->state == NULL` (construction window), underflow clamps `memory_usage` to 0, and `memset` no longer uses `void*` arithmetic.
- ~~**Cross-sandbox function hazard**~~ **FIXED (guard)**: pushing a `LuaSandboxFunction` into a different sandbox now raises E_WARNING "Unable to pass a LuaSandboxFunction to a different sandbox" instead of silently resolving the wrong/nil function; full cross-sandbox transfer remains unsupported.
- ~~**Deterministic `math.random`**~~ **INTENTIONAL (upstream parity)**: fresh sandboxes share seed 0 until `math.randomseed()` is called, so untrusted code sees an identical sequence per process — verified identical to upstream wikimedia/mediawiki-php-luasandbox and left unchanged for reproducibility.

Test-suite gaps (add `.phpt` if behavior changes are made): `active_count` accounting (no PHP-visible consumer exists yet, so a test is only meaningful after exposing the counter).

Covered by tests added in this pass: `tests/wrapphpfunction-noargs.phpt` (F1 regression), `tests/cross-sandbox-function.phpt` (F7b guard), `tests/ini_script-custom.phpt` (was a silent no-op due to a directive-name typo), `tests/ini_script-broken.phpt` (F2: broken ini_script now raises a catchable E_WARNING and the state stays usable).
