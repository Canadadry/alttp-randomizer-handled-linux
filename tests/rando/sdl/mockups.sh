#!/bin/sh
# Usage: mockups.sh ALTTPR UI_CLI SD_FIXTURE THEME OUT
# Checks design/mockups/*.svg: a setup: line (exit 1) without python3, node, npm, the playwright
# module or a launchable Chromium; then mockup_text.py's self-test; then mockup_render.js, which
# proves itself on planted SVGs and renders every mockup offline into OUT/render/. OUT is emptied
# first. One "mockups: <name>: <problem>" line per failure, exit 1 if any.
set -eu
set -f
export LC_ALL=C

usage() {
	echo "usage: mockups.sh ALTTPR UI_CLI SD_FIXTURE THEME OUT" >&2
	exit 2
}

[ $# -eq 5 ] || usage
[ -n "$5" ] || usage
out="$5"
here="$(cd "$(dirname "$0")" && pwd)"
mockups="$here/../../../design/mockups"

setup() {
	echo "setup: $*" >&2
	exit 1
}

for tool in python3 node npm; do
	command -v "$tool" >/dev/null 2>&1 || setup "$tool not found on PATH"
done
NODE_PATH="$(npm root -g)"
export NODE_PATH
if [ -z "${PLAYWRIGHT_BROWSERS_PATH:-}" ] && [ -d /opt/pw-browsers ]; then
	PLAYWRIGHT_BROWSERS_PATH=/opt/pw-browsers
	export PLAYWRIGHT_BROWSERS_PATH
fi
node -e "require('playwright')" >/dev/null 2>&1 ||
	setup "the playwright module is not found under $NODE_PATH (npm install -g playwright)"
node -e "require('playwright').chromium.launch().then(b => b.close()).catch(() => process.exit(1))" >/dev/null 2>&1 ||
	setup "headless Chromium does not launch (npx playwright install chromium)"

rm -rf "$out"
mkdir -p "$out"

python3 -I "$here/mockup_text.py" --self-test
set +f
node "$here/mockup_render.js" "$out/render" "$mockups"/*.svg
