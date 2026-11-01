# Sourced, not run. discover_cases <cases_dir> <program> [<case>...]
# Prints one case name per line; returns 2, printing nothing on stdout and
# "<program>: no such case: <name>" on stderr, when a named case lacks config.json.
# Prints nothing and returns 0 when no case exists.
discover_cases() {
	_dc_dir="$1"
	_dc_prog="$2"
	shift 2
	if [ "$#" -eq 0 ]; then
		for _dc_case in "$_dc_dir"/*/; do
			if [ -f "$_dc_case/config.json" ]; then
				_dc_case="${_dc_case%/}"
				echo "${_dc_case##*/}"
			fi
		done
		return 0
	fi
	for _dc_case in "$@"; do
		if [ ! -f "$_dc_dir/$_dc_case/config.json" ]; then
			echo "$_dc_prog: no such case: $_dc_case" >&2
			return 2
		fi
	done
	printf '%s\n' "$@"
}
