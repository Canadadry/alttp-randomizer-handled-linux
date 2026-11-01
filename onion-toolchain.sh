#!/bin/sh
# Usage: onion-toolchain.sh <command> [<arg>...]
# Runs the command inside Onion's toolchain image (docs/toolchain.md), as root,
# with this repository mounted at /root/workspace, in the directory matching
# the caller's (the mount root when the caller is outside the repository),
# and CROSS_COMPILE exported from the image's /root/.bashrc.
# Exit 1 with one `setup:` line when docker is missing or its daemon does not
# answer; otherwise the command's exit status.
set -eu

image=aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132
mount=/root/workspace

[ $# -gt 0 ] || { echo "usage: onion-toolchain.sh <command> [<arg>...]" >&2; exit 2; }

if ! command -v docker >/dev/null 2>&1; then
	echo "setup: docker not found on PATH (install the docker client)" >&2
	exit 1
fi
if ! docker info >/dev/null 2>&1; then
	echo "setup: the docker daemon does not answer (start dockerd)" >&2
	exit 1
fi

root="$(cd "$(dirname "$0")" && pwd -P)"
here="$(pwd -P)"
case "$here/" in
"$root"/*) workdir="$mount${here#"$root"}" ;;
*) workdir="$mount" ;;
esac

docker image inspect "$image" >/dev/null 2>&1 || docker pull "$image" >&2

exec docker run --rm -v "$root":"$mount" -w "$workdir" "$image" \
	/bin/bash -c 'source /root/.bashrc && exec "$@"' onion-toolchain "$@"
