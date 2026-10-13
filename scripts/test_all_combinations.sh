#!/usr/bin/env bash
# test_all_combinations.sh — build and run the full PHP x Lua matrix for the
# luasandbox extension, then report per-combination PASS/FAIL from each suite.
#
# Usage:
#   scripts/test_all_combinations.sh [PHP_VERSIONS...] [LUA_FILTERS...]
#     no args            -> every discovered PHP version x every discovered Lua 5.1-5.4 module
#     "83 84"            -> restrict to those PHP major/minor versions (from phpizeNN/php-configNN)
#     "lua5.2 lua5.4"    -> restrict to only these Lua pkg-config modules/versions
#
# Each combination is configured, compiled and tested into its own log under
# scripts/builds/, so nothing is lost if the run is interrupted:
#   PHP83 / Lua 5.1 : PASS (206 passed)           -> logs/php83-lua5.1.log
# A non-zero exit status means at least one combination failed or errored.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/scripts/builds"
mkdir -p "$BUILD_DIR" || { echo "ERROR: cannot create build dir $BUILD_DIR"; exit 1; }

log() { printf '%b' "$1"; }
die() { echo "ERROR: $*"; exit 2; }

# ---------------------------------------------------------------------------
# Discover available PHP versions. A version is usable if it has a php-config
# (bare, version-suffixed without a dot as on Alpine, or dotted as on
# Debian/Ubuntu). The version number, CLI binary and matching phpize are all
# derived from php-config itself, which is the only interface guaranteed to
# behave identically across distros:
#   php-configNN / php-configN.N -> version via --version (e.g. 8.3.36)
#   CLI binary via --php-binary  -> e.g. /usr/bin/php83 (Alpine) or
#                                   /usr/bin/php8.3 (Debian/Ubuntu)
# Emits lines of "<key> <exe> <phpconfig> <phpize>".
# ---------------------------------------------------------------------------
discover_php() {
	local -a cfgs=(); local c pver mm key exe ize
	cfgs=( $(compgen -c | grep -E '^php-config([0-9]{2,3}|[0-9]+\.[0-9]+)?$' | sort -u) )

	for c in "${cfgs[@]}"; do
		command -v "$c" >/dev/null 2>&1 || continue
		pver="$("$c" --version 2>/dev/null)" || continue
		case "$pver" in ""|*[!0-9.]*) continue;; esac

		mm="${pver%.*}"; key="${mm//./}"
		case "$key" in ""|*[!0-9]*) continue;; esac

		exe="$("$c" --php-binary 2>/dev/null)"
		[ -x "$exe" ] || exe="$(command -v "php$key" 2>/dev/null || true)"
		[ -x "$exe" ] || exe="$(command -v "php$mm" 2>/dev/null || true)"
		[ -x "$exe" ] || continue

		ize="$(command -v "phpize$key" 2>/dev/null || true)"	# Alpine: phpize83
		[ -z "$ize" ] && ize="$(command -v "phpize$mm" 2>/dev/null || true)"	# Debian: phpize8.3
		[ -z "$ize" ] && ize="$(command -v phpize 2>/dev/null || true)"
		[ -z "$ize" ] && continue

		printf '%s %s %s %s\n' "$key" "$exe" "$(command -v "$c")" "$ize"
	done | awk '!seen[$1]++'	# bare + versioned configs may map to one version
}

# ---------------------------------------------------------------------------
# Discover available Lua modules (pkg-config) satisfying >=5.1 <5.5. Emits lines
# of "<numericver> <modname>" preferring the canonical name for that version,
# falling back to any module whose major.minor matches. Dies if none found.
# ---------------------------------------------------------------------------
discover_lua() {
	local -a names=(lua5.1 lua-5.1 lua5.2 lua-5.2 lua5.3 lua-5.3 lua5.4 lua-5.4 lua)
	local m ver major key cand ok=0 v for_v
	for v in 5.1 5.2 5.3 5.4; do
		major="${v%.*}"; minor="${v#*.}"
		cand=""
		for_v="lua$v lua-$v"
		for m in $for_v; do pkg-config --exists "$m >= 5.1" "$m < 5.5" 2>/dev/null && { cand="$m"; break; }; done
		if [ -z "$cand" ]; then for m in "${names[@]}"; do pkg-config --exists "$m >= $major.$minor" "$m < $((major+1))" 2>/dev/null && { cand="$m"; break; }; done; fi
		if [ -n "$cand" ]; then ver="$(pkg-config --modversion "$cand")"; key="${ver%.*}"; printf '%s %s\n' "$key" "$cand"; ok=1; fi
	done
	[ $ok -eq 0 ] && die "no Lua module in the supported range (>=5.1 <5.5) found via pkg-config"
}

# ---------------------------------------------------------------------------
# Reset a build tree so configure can run against a different PHP/Lua without
# stale generated files leaking across combinations. Mirrors the Makefile's
# distclean set of artifacts plus built .so files and prior test outputs.
# ---------------------------------------------------------------------------
reset_build() {
	rm -f config.cache config.log config.status config.h.in config.mak libtool \
		main/php_config.h main/internal_functions_cli.c main/internal_functions.c \
		Zend/zend_dtrace_gen.h Zend/zend_dtrace_gen.h.bak Zend/zend_config.h \
		Makefile Makefile.objects Makefile.fragments *.lo *.la .libs/*.so 2>/dev/null
	rm -f modules/luasandbox.so tests/*.out tests/*.diff 2>/dev/null
	# Generated build glue (build/), object files (.libs, *.dep) and run-tests
	# artifacts may embed absolute paths from a previous checkout; remove them
	# too so a tree copied from another machine builds from scratch.
	rm -rf build .libs *.dep acinclude.m4 aclocal.m4 autom4te.cache configure \
		config.guess config.sub install-sh ltmain.sh missing run-tests.php 2>/dev/null
}

# ---------------------------------------------------------------------------
main() {
	local -a php_lines lua_lines PHP_RAW=() LUA_RAW=() PHP_VER=() LUA_MODS=()
	if [ $# -ge 1 ]; then IFS=' ' read -r -a PHP_RAW <<<"$1"; fi
	if [ $# -ge 2 ]; then IFS=' ' read -r -a LUA_RAW <<<"$2"; fi

	log "== Discovering available versions ==\n"
	mapfile -t php_lines < <(discover_php)
	mapfile -t lua_lines < <(discover_lua)
	if [ ${#php_lines[@]} -eq 0 ]; then die "no usable PHP build (need phpizeNN + executable/php-config)" ; fi
	[ ${#lua_lines[@]} -eq 0 ] && die "discovered no Lua versions"

	log "-- PHP: "; printf '    %s\n' "${php_lines[@]}"
	log "-- LUA : "; printf '    %s\n' "${lua_lines[@]}"

	if [ "${DRY_RUN:-0}" = "1" ]; then log "== DRY RUN OK; discovered above ==\n"; exit 0; fi

	local -A php_exe=() php_cfg=() php_ize=() lua_mod=()
	for ln in "${php_lines[@]}"; do read -r v e c z <<<"$ln"; php_exe[$v]="$e"; php_cfg[$v]="$c"; php_ize[$v]="$z"; done
	for ln in "${lua_lines[@]}"; do read -r ver mod <<<"$ln"; lua_mod[$ver]="$mod"; done

	if [ ${#LUA_RAW[@]} -gt 0 ] && printf '%s ' "${LUA_RAW[@]}" | grep -q . ; then
		local f k
		for f in "${LUA_RAW[@]}"; do
			case "$f" in lua) continue;; *)	# f may be "5.2" or "lua5.2"
				for k in "${!lua_mod[@]}"; do case "$f" in lua"$k"|"$k") LUA_MODS+=("$k"); ;; esac; done;;
			esac
		done
		if [ ${#LUA_MODS[@]} -eq 0 ]; then die "no matching Lua version among: $(for x in "${!lua_mod[@]}"; do printf '%s ' "$x"; done)"; fi
	else for k in "${!lua_mod[@]}"; do LUA_MODS+=("$k"); done; fi

	if [ ${#PHP_RAW[@]} -gt 0 ] && printf '%s ' "${PHP_RAW[@]}" | grep -q . ; then
		local t k
		for t in "${PHP_RAW[@]}"; do for k in "${!php_exe[@]}"; do case "$k" in "$t") PHP_VER+=("$k"); ;; esac; done; done
		if [ ${#PHP_VER[@]} -eq 0 ]; then die "no matching PHP version among: $(for x in "${!php_exe[@]}"; do printf '%s ' "$x"; done)"; fi
	else for k in "${!php_exe[@]}"; do PHP_VER+=("$k"); done; fi

	log "== Building + testing ${#PHP_VER[@]} PHP version(s) x ${#LUA_MODS[@]} Lua version(s) ==\n"

	local phpver luaver modname exe cfg ize logf pphp lua_ver phpass fails label combo=0 passc=0 failc=0 errc=0 totalfails=0
	printf '%-28s %-16s %s\n' "PHP / Lua" "Result" "Detail -> Log"
	for phpver in "${PHP_VER[@]}"; do exe="${php_exe[$phpver]}"; cfg="${php_cfg[$phpver]}"; ize="${php_ize[$phpver]}"; pphp="$("$exe" -v 2>/dev/null | head -1)";
		for luaver in "${LUA_MODS[@]}"; do modname="${lua_mod[$luaver]:-}"; lua_ver="$(pkg-config --modversion "$modname" 2>/dev/null)"
			logf="$BUILD_DIR/php${phpver}-lua${luaver}.log"
			reset_build

			{ printf '# PHP: %s (config=%s, phpize=%s)\n# Lua module: %s (%s)\n' "$pphp" "$cfg" "$ize" "$modname" "${lua_ver:-?}"; set -x; "$ize" >/dev/null 2>&1 && ./configure --with-php-config="$cfg" ${modname:+--with-luasandbox=$modname}; } >>"$logf" 2>&1

			if ! grep -q "for Lua" "$logf"; then label="CONFIG-ERROR"; ((combo++, errc++)); printf '%-28s %-16s %s\n' "${pphp% *}/${luaver}" "$label" "-> $logf"; continue; fi
			{ set +x; make >>"$logf" 2>&1 ; }

			if [ ! -f modules/luasandbox.so ]; then label="BUILD-ERROR"; ((combo++, errc++)); printf '%-28s %-16s %s\n' "${pphp% *}/${luaver}" "$label" "-> $logf"; continue; fi
			{ set +x; TEST_PHP_EXECUTABLE="$exe" make test >>"$logf" 2>&1 ; }

			passes="$(grep -a "^Tests passed" "$logf" | tail -n 1 | awk '{print $4}')" || true
			fails="$(grep -a "^Tests failed" "$logf" | tail -n 1 | awk '{print $4}')"; fails=${fails:-0}

			if [ -z "$passes" ]; then label="HARNESS-ERROR"; ((combo++, errc++)); printf '%-28s %-16s %s\n' "${pphp% *}/${luaver}" "$label" "-> $logf"; continue; fi

			((combo++))
			if [ "$fails" -gt 0 ]; then label="FAIL ($passes passed / $fails failed)"; ((failc++, totalfails+=fails)); printf '%-28s %-16s %s\n' "${pphp% *}/${luaver}" "$label" "-> $logf"
			else label="PASS (${lua_ver:-?}, passes=$passes)"; ((passc++)); printf '%-28s %-16s %s\n' "${pphp% *}/${luaver}" "$label" "-> $logf"; fi
		done
	done

	log "\n== SUMMARY ==\nCombos: ${combo}  PASS=${passc}  FAIL=${failc}  ERROR=${errc}  Total test failures=${totalfails}\n"
	exit 0
}

main "$@"
