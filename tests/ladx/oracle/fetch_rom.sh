#!/bin/sh
# Usage: fetch_rom.sh [<url>]
# Downloads the LADX English 1.0 ROM into tmp/ladx/base.gbc from the URL given as the argument,
# else LADX_ROM_URL; refuses any other SHA-1. The URL is never read from a file, written or echoed.
set -eu
export LC_ALL=C

LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_top="$(cd "$oracle_dir/../../.." && pwd)"
target="$repo_top/tmp/ladx/base.gbc"

if [ "$#" -gt 1 ]; then
	echo "usage: fetch_rom.sh [<url>]" >&2
	exit 2
fi

if ! command -v curl >/dev/null 2>&1; then
	echo "setup: fetch_rom: curl not found" >&2
	exit 2
fi
if command -v sha1sum >/dev/null 2>&1; then
	sha1_of() { sha1sum "$1" | cut -d ' ' -f 1; }
elif command -v shasum >/dev/null 2>&1; then
	sha1_of() { shasum -a 1 "$1" | cut -d ' ' -f 1; }
else
	echo "setup: fetch_rom: neither sha1sum nor shasum found" >&2
	exit 2
fi

# fetch_checked <url> <target> <expected sha1>: prints the SHA-1 on stdout and exits 0 when the
# download matches; else leaves no target and no part, prints a setup: line and returns 2.
fetch_checked() {
	fc_url="$1"
	fc_target="$2"
	fc_expected="$3"
	fc_part="$fc_target.part"
	mkdir -p "$(dirname "$fc_target")"
	rm -f "$fc_part"
	fc_code=000
	fc_rc=0
	fc_code="$(curl -fsSL --retry 3 -w '%{http_code}' -o "$fc_part" "$fc_url" 2>/dev/null)" || fc_rc=$?
	if [ "$fc_rc" -ne 0 ]; then
		rm -f "$fc_part"
		echo "setup: fetch_rom: download failed (curl exit $fc_rc, HTTP ${fc_code:-000})" >&2
		return 2
	fi
	fc_sha1="$(sha1_of "$fc_part")"
	if [ "$fc_sha1" != "$fc_expected" ]; then
		rm -f "$fc_part"
		echo "setup: fetch_rom: refused, SHA-1 $fc_sha1 is not the LADX English 1.0 ROM ($(printf '%.8s' "$fc_expected")…)" >&2
		return 2
	fi
	mv -f "$fc_part" "$fc_target"
	echo "$fc_sha1"
}

self_test_failed() {
	echo "fetch_rom: self-test failed: $1" >&2
	rm -rf "$st_dir"
	exit 1
}

self_test() {
	st_dir="$(mktemp -d)"
	st_planted="$st_dir/served/planted.bin"
	mkdir -p "$st_dir/served"
	st_i=0
	while [ "$st_i" -lt 64 ]; do
		printf 'fetch_rom planted line %040d\n' "$st_i" >>"$st_planted"
		st_i=$((st_i + 1))
	done
	[ "$(wc -c <"$st_planted" | tr -d ' ')" -eq 4096 ] || self_test_failed "planted file is not 4 KiB"
	st_url="file://$st_planted"
	st_target="$st_dir/out/base.gbc"

	st_rc=0
	(fetch_checked "$st_url" "$st_target" "$LADX_SHA1") >"$st_dir/refuse.out" 2>&1 || st_rc=$?
	[ "$st_rc" -eq 2 ] || self_test_failed "wrong SHA-1 not refused (exit $st_rc)"
	[ ! -e "$st_target" ] || self_test_failed "wrong SHA-1 left the target"
	[ ! -e "$st_target.part" ] || self_test_failed "wrong SHA-1 left the part"
	! grep -qF "$st_url" "$st_dir/refuse.out" || self_test_failed "refusal echoed the URL"
	grep -q '^setup: fetch_rom: refused' "$st_dir/refuse.out" || self_test_failed "refusal without its setup: line"

	st_sha1="$(sha1_of "$st_planted")"
	st_rc=0
	(fetch_checked "$st_url" "$st_target" "$st_sha1") >"$st_dir/accept.out" 2>"$st_dir/accept.err" || st_rc=$?
	[ "$st_rc" -eq 0 ] || self_test_failed "right SHA-1 not accepted (exit $st_rc)"
	[ -f "$st_target" ] || self_test_failed "right SHA-1 left no target"
	[ ! -e "$st_target.part" ] || self_test_failed "right SHA-1 left the part"
	[ "$(cat "$st_dir/accept.out")" = "$st_sha1" ] || self_test_failed "accept stdout is not the SHA-1 alone"
	[ "$(wc -l <"$st_dir/accept.out" | tr -d ' ')" -eq 1 ] || self_test_failed "accept stdout is not one line"
	! grep -qF "$st_url" "$st_dir/accept.out" "$st_dir/accept.err" || self_test_failed "accept echoed the URL"

	rm -rf "$st_dir"
}

self_test

if [ -f "$target" ] && [ "$(sha1_of "$target")" = "$LADX_SHA1" ]; then
	echo "$LADX_SHA1"
	exit 0
fi

url="${1:-${LADX_ROM_URL:-}}"
if [ -z "$url" ]; then
	echo "setup: fetch_rom: no URL (argument or LADX_ROM_URL) and no ROM in tmp/ladx/" >&2
	exit 2
fi

fetch_checked "$url" "$target" "$LADX_SHA1"
