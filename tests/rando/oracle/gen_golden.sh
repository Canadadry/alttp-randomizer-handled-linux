#!/bin/sh
# Usage: gen_golden.sh [--fresh] [--out <dir>] [<case>...]
# Without --out, each case's golden is written back into tests/rando/cases/<case>/, and the logic fixtures
# (extract_logic.php) and the pool fixtures pool_lists.txt and prepared_pools.txt (dump_pools.php, always over
# every case) into tests/rando/oracle/;
# with --out, all go under <dir>.
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

names="$(discover_cases "$cases_dir" gen_golden "$@")"
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

for name in "$@"; do
	php "$oracle_dir/gen_case.php" "$vt_dir" "$cases_dir/$name" "${out:-$cases_dir}/$name"
done

# After the cases: the dump checks its draws against each golden trace, so it must read the ones just written.
all_cases="$(discover_cases "$cases_dir" gen_golden)"
dump_dirs=
for name in $all_cases; do
	dir="$cases_dir/$name"
	for named in "$@"; do
		if [ "$named" = "$name" ]; then dir="${out:-$cases_dir}/$name"; fi
	done
	dump_dirs="$dump_dirs $dir"
done
set -f
php "$oracle_dir/dump_pools.php" "$vt_dir" "${out:-$oracle_dir}" $dump_dirs
set +f
