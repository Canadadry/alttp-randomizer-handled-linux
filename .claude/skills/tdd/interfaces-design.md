# Interface Design for Testability

Good interfaces make testing natural. Names are illustrative.

1. **Accept inputs, don't look for them**

   ```python
   # Testable: the caller says where everything is
   def import_orders(csv_path, db): ...

   # Hard to test: hidden dependency on the environment
   def import_orders(): rows = open("/var/data/orders.csv"); db = connect(os.environ["DB_URL"])
   ```

2. **Return results, don't only print them**

   A function that computes a report returns it (a list of rejected rows); the command-line or HTTP layer prints it and sets the exit status or response code.

3. **Small surface area**
   - Fewer entry points = fewer tests needed
   - Fewer parameters = simpler test setup

4. **Keep the core pure; push I/O to the top**

   Parsing, validating and computing work on plain values; reading files, calling services and writing to the database happen in a thin top layer.

5. **Fail at the boundary**

   Every entry point checks what it requires (a known format, a value in range, a reference that exists) and stops with a message naming the input, so bad input fails at once, not as a wrong result three steps later.
