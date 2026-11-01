#!/bin/sh
# usage: no_claude_branch.sh --install
#        no_claude_branch.sh <pre-push arguments>   (ref lines on stdin)
# Git pre-push hook refusing any push of a claude/* branch, local or remote:
# a branch is named after the work it carries (CLAUDE.md § Branches).
# --install writes the hook into the repository's hooks directory (shared by
# every worktree); the SessionStart hook of .claude/settings.json runs it.
# An existing pre-push hook that is not this one is left alone.

set -u

if [ "${1:-}" = "--install" ]; then
    hooks=$(git rev-parse --git-common-dir 2>/dev/null) || exit 0
    hooks=$hooks/hooks
    mkdir -p "$hooks"
    if [ -f "$hooks/pre-push" ] && ! grep -q no_claude_branch.sh "$hooks/pre-push"; then
        echo "no_claude_branch: $hooks/pre-push exists and is not ours, left alone" >&2
        exit 0
    fi
    cat >"$hooks/pre-push" <<'HOOK'
#!/bin/sh
exec "$(git rev-parse --show-toplevel)/.claude/hooks/no_claude_branch.sh" "$@"
HOOK
    chmod +x "$hooks/pre-push"
    exit 0
fi

status=0
while read -r local_ref local_sha remote_ref remote_sha; do
    for ref in "$local_ref" "$remote_ref"; do
        case $ref in
            refs/heads/claude/*)
                echo "pre-push: refused, $ref is a claude/* branch." >&2
                echo "pre-push: name the branch after the work (<type>/<short-topic>, or plan-<N>-<topic>):" >&2
                echo "pre-push:   git branch -m <name> && git push -u origin <name>" >&2
                status=1
                break
                ;;
        esac
    done
done
exit $status
