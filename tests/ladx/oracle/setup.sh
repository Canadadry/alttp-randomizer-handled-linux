#!/bin/sh
# Usage: setup.sh [--fresh]
# Clones LADXR at its pin into LADXR_DIR (default tests/ladx/oracle/ladxr, git-ignored), applies
# ladxr.patch when it exists, and prints the checked-out commit and the Python version.
# Needs git, the network for the first fetch, and python3 >= 3.11; else a setup: line and exit 2.
set -eu
export LC_ALL=C

LADXR_URL=https://github.com/daid/LADXR.git
LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef
PYTHON_EXPECTED=3.13

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
ladxr_dir="${LADXR_DIR:-$oracle_dir/ladxr}"

fresh=0
while [ "$#" -gt 0 ]; do
	case "$1" in
	--fresh) fresh=1 ;;
	*)
		echo "usage: setup.sh [--fresh]" >&2
		exit 2
		;;
	esac
	shift
done

if ! command -v git >/dev/null 2>&1; then
	echo "setup: git not found" >&2
	exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
	echo "setup: python3 not found" >&2
	exit 2
fi
python_version="$(python3 -c 'import sys; print("%d.%d.%d" % sys.version_info[:3])')"
if ! python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))'; then
	echo "setup: python $python_version, the oracle needs 3.11 or later" >&2
	exit 2
fi

if [ "$fresh" -eq 1 ]; then rm -rf "$ladxr_dir"; fi
if [ ! -d "$ladxr_dir/.git" ]; then
	mkdir -p "$ladxr_dir"
	git -C "$ladxr_dir" init -q
	git -C "$ladxr_dir" remote add origin "$LADXR_URL"
fi
if ! git -C "$ladxr_dir" cat-file -e "$LADXR_PIN^{commit}" 2>/dev/null; then
	if ! git -C "$ladxr_dir" fetch -q --depth 1 origin "$LADXR_PIN"; then
		echo "setup: cannot fetch LADXR $LADXR_PIN from $LADXR_URL (network?)" >&2
		exit 2
	fi
fi
git -C "$ladxr_dir" -c advice.detachedHead=false checkout -q --force "$LADXR_PIN"
git -C "$ladxr_dir" reset -q --hard "$LADXR_PIN"
git -C "$ladxr_dir" clean -q -fdx

patched=
if [ -f "$oracle_dir/ladxr.patch" ]; then
	git -C "$ladxr_dir" apply "$oracle_dir/ladxr.patch"
	patched=", ladxr.patch applied"
fi

echo "setup: ladxr $(git -C "$ladxr_dir" rev-parse HEAD), python $python_version$patched"
case "$python_version" in
"$PYTHON_EXPECTED".*) ;;
*) echo "setup: warning: python $python_version, the fixtures were made with $PYTHON_EXPECTED" >&2 ;;
esac
