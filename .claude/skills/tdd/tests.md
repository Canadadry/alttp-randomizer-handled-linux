# Good and Bad Tests

Tests follow the project's layout and runner, run by `make test` (`CLAUDE.md` § Checks); the existing tests are the prior art. The examples below are Python and illustrative.

## Good Tests

**Integration-style**: test through the module's entry point or the functions it exposes, on an input the test builds.

```python
# GOOD: observable behavior, on a small input built by the test
def test_rejects_rows_without_an_email(self):
    report = import_orders(write_csv(self.tmp, ["name,email", "Ann,", "Bob,bob@example.com"]), self.db)
    self.assertEqual(report.rejected_lines, [2])
```

Characteristics:

- Tests behavior callers care about (what lands in the output, what the command reports)
- Uses only what the module exposes
- Survives internal refactors
- Name describes WHAT, not HOW
- One logical assertion per test

Build the input in the test (a temporary file, a few rows, a fake client), then run the code on it and check its output. Never read production data or reach the network.

## Bad Tests

**Implementation-detail tests**: coupled to internal structure.

```python
# BAD: asserts on a private helper's intermediate state
def test_parsed_rows_cache(self):
    importer = Importer(path)
    importer._parse()
    self.assertEqual(len(importer._rows), 3)
```

Red flags:

- Reaching into private helpers or intermediate data structures
- Asserting on call counts or call order
- Test breaks when refactoring without behavior change
- Test name describes HOW, not WHAT
- Verifying through a side channel instead of the output
- Depending on data or services outside the test

## Assertions and Tests: Complementary Layers

Tests verify behavior from the outside. Assertions (an exception with a message, `assert` for internal invariants) enforce contracts from the inside. Both are needed.

```python
# Without a check: an unknown currency silently converts at rate 0, a wrong total appears later
amount = value * RATES.get(currency, 0)

# With a check: fails at the violated contract
if currency not in RATES:
    raise ValueError(f"line {line}: unknown currency {currency!r}")
```

Rule of thumb: if a test setup must respect a constraint to call a function, that constraint is checked inside the function.
