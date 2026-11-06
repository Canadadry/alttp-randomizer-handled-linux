#!/bin/sh
# Usage: gen_golden.sh [--fresh] [--out <dir>] [<case>...]
# A <case> names a directory of tests/alttp/cases/ or of tests/alttp/combos/; with none, every case of both.
# Without --out, each case's golden is written back into its own directory, and the logic fixtures
# (extract_logic.php), the default text table text_default.txt (text_default.php), the hint and string
# alternative counts hint_tables.txt (hint_tables.php), the credits scene segments credit_tables.txt
# (credit_tables.php), the seed-dependent text encodings and hint names text_tables.txt (text_tables.php)
# and the pool fixtures
# pool_lists.txt and prepared_pools.txt (dump_pools.php, always over every case of cases/ only) into
# tests/alttp/oracle/; with --out, all go under <dir>, a combination at <dir>/combos/<case>.
# Never called by `make test` or `make e2e`: regenerating goldens is a deliberate act (CLAUDE.md).
set -eu
export LC_ALL=C

VT_URL=https://github.com/sporchia/alttp_vt_randomizer.git
VT_PIN=219fcafd029dab597b8db400efafd8f56f8b4edb
PHP_EXPECTED=8.3

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
rando_tests_dir="$(dirname "$oracle_dir")"
. "$rando_tests_dir/cases.sh"
cases_dir="$rando_tests_dir/cases"
combos_dir="$rando_tests_dir/combos"
vt_dir="${VT_DIR:-$oracle_dir/vt}"

usage() {
	echo "usage: gen_golden.sh [--fresh] [--out <dir>] [<case>...]" >&2
	exit 2
}

fresh=0
out=
while [ "$#" -gt 0 ]; do
	case "$1" in
	--fresh) fresh=1 ;;
	--out)
		[ "$#" -ge 2 ] || usage
		out="$2"
		shift
		;;
	-*) usage ;;
	*) break ;;
	esac
	shift
done

names="$(locate_cases "$cases_dir" "$combos_dir" gen_golden "$@")"
if [ -z "$names" ]; then
	echo "gen_golden: no cases" >&2
	exit 2
fi
set -f
set -- $names
set +f

if [ -n "$out" ]; then
	mkdir -p "$out"
	out="$(cd "$out" && pwd)"
fi

if [ "$fresh" -eq 1 ]; then rm -rf "$vt_dir"; fi
if [ ! -d "$vt_dir/.git" ]; then
	mkdir -p "$vt_dir"
	git -C "$vt_dir" init -q
	git -C "$vt_dir" remote add origin "$VT_URL"
fi
if ! git -C "$vt_dir" cat-file -e "$VT_PIN^{commit}" 2>/dev/null; then
	git -C "$vt_dir" fetch -q --depth 1 origin "$VT_PIN"
fi
git -C "$vt_dir" -c advice.detachedHead=false checkout -q --force "$VT_PIN"
git -C "$vt_dir" reset -q --hard "$VT_PIN"
# -fd without -x keeps VT's git-ignored vendor/ between runs; --fresh starts from nothing.
git -C "$vt_dir" clean -q -fd
git -C "$vt_dir" apply "$oracle_dir/vt.patch"

# ext-bcmath: this container's PHP lacks it, and only libraries with non-bcmath fallbacks reference it.
(cd "$vt_dir" && COMPOSER_ALLOW_SUPERUSER=1 composer install -q --no-dev --no-interaction \
	--ignore-platform-req=ext-bcmath)

php_version="$(php -r 'echo PHP_VERSION;')"
echo "gen_golden: php $php_version, $(COMPOSER_ALLOW_SUPERUSER=1 composer --version 2>/dev/null | head -n 1), vt $(git -C "$vt_dir" rev-parse HEAD)"
case "$php_version" in
"$PHP_EXPECTED".*) ;;
*) echo "gen_golden: warning: php $php_version, the goldens were made with $PHP_EXPECTED" >&2 ;;
esac

php "$oracle_dir/prng_check.php" "$vt_dir"
php "$oracle_dir/extract_logic.php" "$vt_dir" "${out:-$oracle_dir}"
php "$oracle_dir/text_default.php" "$vt_dir" "${out:-$oracle_dir}"
php "$oracle_dir/hint_tables.php" "$vt_dir" "${out:-$oracle_dir}"
php "$oracle_dir/credit_tables.php" "$vt_dir" "${out:-$oracle_dir}"
php "$oracle_dir/text_tables.php" "$vt_dir" "${out:-$oracle_dir}"

for dir in "$@"; do
	name="${dir##*/}"
	dest="${out:-$cases_dir}/$name"
	if [ "$dir" = "$combos_dir/$name" ]; then dest="${out:-$rando_tests_dir}/combos/$name"; fi
	php "$oracle_dir/gen_case.php" "$vt_dir" "$dir" "$dest"
done

# After the cases: the dump checks its draws against each golden trace, so it must read the ones just written.
all_cases="$(discover_cases "$cases_dir" gen_golden)"
dump_dirs=
for name in $all_cases; do
	dir="$cases_dir/$name"
	for named in "$@"; do
		if [ "$named" = "$cases_dir/$name" ]; then dir="${out:-$cases_dir}/$name"; fi
	done
	dump_dirs="$dump_dirs $dir"
done
set -f
php "$oracle_dir/dump_pools.php" "$vt_dir" "${out:-$oracle_dir}" $dump_dirs
set +f
