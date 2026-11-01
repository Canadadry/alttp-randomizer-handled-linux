# ALttPR on Miyoo

An on-device *A Link to the Past Randomizer* seed generator for the **Miyoo
Mini Flip running OnionOS**: an Onion app, driven by the d-pad, that reads
your Japanese 1.0 ROM, generates a seed with every alttpr.com setting, and
writes a patched `.sfc` you play with Onion's own SNES core. One seed is one
ROM, so each seed keeps its own save. A History tab lists the seeds and
deletes a seed's ROM, save and save states. A PC build (SDL) shows the same
app on Linux and macOS.

The generator is a C port of VT (`sporchia/alttp_vt_randomizer`, frozen at
`219fcafd`), checked against VT itself. No Nintendo ROM is ever in this
repository.

Status: being built; see the plan in `docs/`.

## Development

Needs only `gcc`, `make` and `python3`; nothing to install.

```bash
make test
```

```bash
make check
```

`make test` is the fast loop (unit tests); `make check` is everything that
must be green before a push.

## Working with Claude

### Skills you run

| Skill | Use it to |
|-------|-----------|
| `/grill-me` | Get interviewed about a plan or design until every decision is made |
| `/plan-write` | Turn that conversation into `docs/plan-<N>.md` |
| `/plan-converge <plan>` | Sharpen the plan with review rounds until they come back empty |
| `/plan-pilot <plan>` | Implement the whole plan unattended, step by step, then open the PR |
| `/fix-bug <symptom>` | Fix one bug: proven cause, failing test, minimal fix, independent check |

Run `plan-converge` and `plan-pilot` each in a **new session**: the plan file
is all they know, so everything decided while grilling must be in it.

### Skills the others call

`to-prd` (one PRD per plan step), `prd-autopilot` (implements the PRD queue),
`tdd` (red-green-refactor), `check-net` (runs `make check`, drives failures
back to green), `help-me-debug` (proves a bug's cause), `git-commit`,
`plan-review`, `warn-review` (design smells at each phase end), `guideline`
(the stand-in human during unattended runs).

Follow a running plan by reading `progress.md` at the root of its branch.
