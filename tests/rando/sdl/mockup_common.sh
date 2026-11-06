# Sourced by mockups.sh and theme_compare.sh (POSIX sh, set -f in effect).
# browser_setup: a setup: line and exit 1 without python3, node, npm, the playwright module or a
# launchable Chromium; exports NODE_PATH and PLAYWRIGHT_BROWSERS_PATH (default /opt/pw-browsers).
# pairing_check PREFIX MOCKUPS STATES: one "PREFIX: <name>: no mockup" / "no pairing" line per
# mockup of MOCKUPS/*.svg and line of STATES without the other; sets paired to the names of STATES
# that have a mockup; returns 1 if any line was printed.

setup() {
	echo "setup: $*" >&2
	exit 1
}

browser_setup() {
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
}

pairing_check() {
	_prefix="$1"
	_mockups="$2"
	_states="$3"
	_status=0
	paired=""
	while IFS='	' read -r _name _rest; do
		[ -n "$_name" ] || continue
		if [ -f "$_mockups/$_name.svg" ]; then
			paired="$paired $_name"
		else
			echo "$_prefix: $_name: no mockup"
			_status=1
		fi
	done <"$_states"
	set +f
	for _svg in "$_mockups"/*.svg; do
		[ -f "$_svg" ] || continue
		_name="$(basename "$_svg" .svg)"
		grep -q "^$_name	" "$_states" || { echo "$_prefix: $_name: no pairing"; _status=1; }
	done
	set -f
	return "$_status"
}
