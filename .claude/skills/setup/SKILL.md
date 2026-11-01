---
name: setup
description: Bootstraps a new, empty project created from this template — picks the stack with the user, scaffolds a minimal skeleton with one passing test, wires make test and make check, fills CLAUDE.md, trims what the project doesn't need, proves both targets green, commits. Run once, right after creating the repository.
---

# setup

Runs on a repository just created from this template: it holds the template's files and no project code. Your job is to leave a project that builds, with one passing test and a green net, so the first `/grill-me` starts on solid ground.

Interactive: the user answers, you write. Questions are asked **in plain text, one at a time** — `AskUserQuestion` is denied in `.claude/settings.json` (unattended runs must never block on it), so don't try it. Whenever a question has a sensible default, propose it so the user can just say yes.

## 1. Check the starting point

- The tree holds only the template's files (`.claude/`, `CLAUDE.md`, `Makefile`, `README.md`, `.gitignore`) and `CLAUDE.md` still says "Template not set up yet". Anything else (project code already there, setup already done) → tell the user and ask whether to go on; never overwrite their files.
- `make test` passes on the template as shipped. Red → report it before changing anything.
- Note the current branch and the remote.
- Note which toolchains are installed in this session (`python3`, `node`, `go`, `cargo`, `java`…): it bounds what can be proven green here.

## 2. Ask, one question at a time

1. **The project**: what it is and who it's for, in one or two sentences.
2. **The stack**: language, and the kind of project (CLI, library, web service, web app…). Propose a default from the answer to 1.
3. **The tools**: for the chosen stack, propose the standard set and let the user amend it — package manager, test runner, formatter, linter, type checker. Keep it to what the stack's community uses by default; nothing exotic.
4. **The net**: beyond lint, format and type checks, anything else that must be green before a push (a build, integration tests…) and what it needs (a service, a container, credentials). "Nothing yet" is a valid answer: the net grows with the project.
5. **Ground rules**: project-specific rules every change must respect, and the goal each one serves (`warn-review` flags violations, `guideline` decides by them). None is a valid answer — they can be added later.
6. **Language**: everything in the repository in English, the conversation in the user's language? Keep, or change.
7. **The plan pipeline** (`grill-me` → `plan-write` → `plan-converge` → `plan-pilot`, and its unattended helpers): keep it all, or only the interactive skills (`grill-me`, `fix-bug`, `git-commit`, `tdd`)?
8. **CI**: a GitHub Actions workflow running `make check` on pushes and pull requests — yes or no?

Then sum up the choices in a short list and get a go-ahead before writing anything.

## 3. Scaffold

The skeleton is the smallest thing the tools can run on — no feature, no speculative structure:

- the manifest of the package manager, with the chosen tools as dev dependencies and their configuration (in the manifest when the tool supports it, rather than one file per tool);
- the source directory with one module holding one trivial function;
- one test of that function, through its public interface, in the runner's usual place — the prior art `tdd` will follow;
- the language's build output and dependency directories added to `.gitignore`.

Install the dependencies so the next step can run them.

## 4. Wire the targets

- **`Makefile`**: `test` runs the project's test runner **and** keeps its dependency on `test-template` (the template's tooling under `.claude/`: the pre-push hook and `plan-pilot`'s scripts). Make sure the project's runner does not also discover the tests under `.claude/` (exclude it in the runner's configuration if needed). `check` depends on `test` plus one target per slow check (`lint`, `format-check`, `typecheck`…) so `make -k check` reports them all. Add `install` for the dependencies. Keep `help` in sync.
- **`CLAUDE.md`**: replace the placeholder and drop the "Template not set up yet" line. § Checks names the real commands, the install step, and what the net needs; § Ground rules lists the rules with their goals, or is deleted; § Language follows answer 6. Keep the sections the skills cite: § Checks, § Ground rules (if kept), § Language, § What we don't do, § Branches, § Plans (if the pipeline is kept). Add a line on the source layout.
- **`README.md`**: the project's name and description, how to install and run the tests; keep § Working with Claude, drop the template's Quickstart.
- **CI** (answer 8): a workflow that installs the toolchain and dependencies, then runs `make check`.

## 5. Trim (only if the pipeline is dropped)

`git rm` `plan-write`, `plan-converge`, `plan-review`, `plan-pilot` (its scripts and tests with it), `prd-autopilot`, `to-prd`, `check-net`, `warn-review`, `guideline`, the `plan-pilot` line of `test-template`, and § Plans of `CLAUDE.md`. Then fix what still names them: `fix-bug` (its unattended mode; its sub-agents' questions go to the user instead of a `guideline` agent), `git-commit` (§ `progress.md` and the unattended callers), `tdd`'s description, `README.md`'s skill lists.

## 6. Prove it

Run `make test`, then `make check`. Both must pass. A red target is fixed with the user (a missing tool, a wrong command), never by weakening a check. A target that cannot run in this session (a toolchain or service missing here) is said plainly, and how to run it is written in `CLAUDE.md` § Checks.

## 7. Commit

`git rm -r .claude/skills/setup` — this skill is used once, and the template stays the reference for the next project. Then run `git-commit` in place; the user holds the gate. One commit, `chore: bootstrap <project name> from the template`, carrying the skill's removal too.

Branch (`CLAUDE.md` § Branches): on a `claude/*` branch, create `chore/bootstrap` first, then push there.

## 8. Next

Print what to run next, in a new session:

```
/grill-me <what you want to build first>
```

then `/plan-write` at the end of the grilling, `/plan-converge docs/plan-1.md`, and `/plan-pilot docs/plan-1.md`.
