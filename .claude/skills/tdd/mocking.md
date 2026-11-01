# When to Use a Test Double

Replace only **system boundaries**, and prefer a real, tiny input over a double:

- External services (third-party APIs, payment, email): replace them with a fake at the boundary.
- Production data: never read in a unit test; build a small input in the test instead.
- Time and randomness: pass a clock or a seed in, rather than patching globals.
- A local dependency that is cheap to run for real (a temporary directory, an in-memory or throwaway database, a real parser): use the real one.

Don't replace:

- Your own modules
- Internal collaborators
- Libraries you can run for real on a small input

## Designing for Test Doubles

**Pass the dependency in**: a module takes its inputs (paths, clients, settings) as arguments or from the environment, never from a hard-coded location, so a test can point it at a temporary directory or a fake.
