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

# locate_cases <cases_dir> <combos_dir> <program> [<case>...]
# Prints one case directory per line: every case of both directories, or each
# named one. Returns 2, printing nothing on stdout, when a named case is in
# neither ("<program>: no such case: <name>") or in both directories.
locate_cases() {
	_lc_cases="$1"
	_lc_combos="$2"
	_lc_prog="$3"
	shift 3
	_lc_in_cases="$(discover_cases "$_lc_cases" "$_lc_prog")"
	_lc_in_combos="$(discover_cases "$_lc_combos" "$_lc_prog")"
	if [ "$#" -eq 0 ]; then
		for _lc_case in $_lc_in_cases; do echo "$_lc_cases/$_lc_case"; done
		for _lc_case in $_lc_in_combos; do echo "$_lc_combos/$_lc_case"; done
		return 0
	fi
	_lc_out=
	for _lc_case in "$@"; do
		_lc_where=
		for _lc_name in $_lc_in_cases; do
			if [ "$_lc_name" = "$_lc_case" ]; then _lc_where="$_lc_cases/$_lc_case"; fi
		done
		for _lc_name in $_lc_in_combos; do
			if [ "$_lc_name" != "$_lc_case" ]; then continue; fi
			if [ -n "$_lc_where" ]; then
				echo "$_lc_prog: case in both ${_lc_cases##*/}/ and ${_lc_combos##*/}/: $_lc_case" >&2
				return 2
			fi
			_lc_where="$_lc_combos/$_lc_case"
		done
		if [ -z "$_lc_where" ]; then
			echo "$_lc_prog: no such case: $_lc_case" >&2
			return 2
		fi
		_lc_out="$_lc_out$_lc_where
"
	done
	printf '%s' "$_lc_out"
}
