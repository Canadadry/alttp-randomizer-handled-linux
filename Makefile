.PHONY: help test test-template check

help:
	@echo "make test           the fast loop: unit tests, on the host, in seconds"
	@echo "make test-template  the template's own tooling tests (.claude/), run by make test"
	@echo "make check          the net: everything that must be green before a push"

# The fast loop. Keep it a few seconds, with no network and no external service.
test: test-template

# The template's tooling under .claude/: the pre-push hook and plan-pilot's
# progress.md hook. Kept apart from the project's own tests.
test-template:
	python3 -m unittest discover -s .claude/hooks/tests
	python3 -m unittest discover -s .claude/skills/plan-pilot/tests

# The net. Add the slow checks here (lint, type check, integration, e2e...).
# A change that keeps it green is correct; one that breaks it is a bug.
check: test
