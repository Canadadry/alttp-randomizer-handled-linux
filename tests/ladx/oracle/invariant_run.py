"""The oracle's re-run of LADXR's C-group calls on a scrambled copy of the ROM state, for the
`invariant` clearance of the Nintendo-bytes check (write_log.py's `I` lines).

As a module: `install(trace, targets, sides, known=None, tracer=None) -> Rerun`, after
read_trace.install. Each target (module, attribute) is wrapped; a call made straight from a
traced section line whose inventory group has side C, and whose own writes hold a computed byte
(an origin below 0), is re-run once on a copy of the ROMWithTables state as it was before the
call:
  - every bank byte and table-entry byte is scrambled (xor a nonzero mask of its source offset,
    else its position), except a byte never written whose source offset is in the C-knowable map
    (`known(source) -> container`, default known_source.build) and a byte an earlier write made
    with a computed origin; a byte an earlier write copied from the source (`src` origin) is
    scrambled too, wherever it came from;
  - the other arguments are deep copies (the object itself when it cannot be copied, like
    LADXR's Settings); the random generators among them and the module's
    global one start from the state the real call started from, and are put back to the state
    it left after the re-run; the ladx_oracle Tracer's lines the re-run adds are dropped;
  - ROM.patch's old-byte check is off on the copy only (the old bytes are written first).
The real call runs first, traced, and alone gives the golden. Then each of its events gets one
flag per byte: whether the copy holds the same byte at the same place (bank offset, or table
entry of the same length) once the re-run returned; all false when the re-run raises or its
arguments cannot be copied; an integer pointer or an empty value gets none. `Rerun.flags`
{event index: flags}, `Rerun.lines()` the record
write_log.py reads (invariant_lines), `Rerun.uninstall()`.

`targets_of(inventory_text)` gives the (module name, attribute) of every call in a row of
generator.py generateRom whose group's side is C (inventory_check.CALLEE).
"""

import copy
import random
import re
import sys
import types

KNOWN = -1
# The standard generator, also when ladx_oracle's Tracer has replaced random.Random.
_RANDOM = next(c for c in random.Random.__mro__ if c.__module__ == "random"
               and c.__name__ == "Random")


def _mask(at):
    return 1 + at * 7 % 255


def sides_of(inventory_text):
    import inventory_check
    named, _ = inventory_check.parse_named(inventory_text)
    return {row["Group"]: row["Side"] for row in named["Groups"]}


def targets_of(inventory_text):
    import inventory_check
    sides = sides_of(inventory_text)
    tables, _ = inventory_check.parse_inventory(inventory_text)
    found = set()
    for row in tables.get("generator.py generateRom", []):
        if sides.get(row["Group"]) != "C":
            continue
        for m in inventory_check.CALLEE.finditer(row["key"]):
            groups = m.groups()
            if groups[0]:
                found.add(("patches." + groups[0], groups[1]))
            else:
                name, attr = groups[2] or groups[4], groups[3] or groups[5]
                found.add((name, attr))
    return sorted(found)


class Rerun:
    def __init__(self, trace, sides, known, tracer):
        self.trace, self.sides, self._known_of, self.tracer = trace, sides, known, tracer
        self.flags = {}
        self.calls = 0
        self.failed = 0
        self.errors = []
        self._restore = []
        self._scrambled_source = None
        self._known = None

    def uninstall(self):
        for owner, name, value in reversed(self._restore):
            setattr(owner, name, value)
        self._restore = []

    def lines(self):
        import write_log
        return write_log.invariant_lines(self.flags)

    def wrap(self, owner, name):
        original = getattr(owner, name)
        rerun = self

        def wrapped(*args, **kwargs):
            if not rerun._wanted(sys._getframe(1)):
                return original(*args, **kwargs)
            return rerun._call(original, args, kwargs)
        wrapped.__name__ = getattr(original, "__name__", name)
        wrapped.__wrapped__ = original
        self._restore.append((owner, name, original))
        setattr(owner, name, wrapped)

    def _wanted(self, frame):
        trace = self.trace
        if not trace.active:
            return False
        section = trace._info(frame.f_code)[2]
        if section is None:
            return False
        group = trace._inventory.get(trace._site(frame, section), "-")
        return self.sides.get(group) == "C"

    def _call(self, original, args, kwargs):
        trace = self.trace
        rom = next((a for a in list(args) + list(kwargs.values()) if hasattr(a, "banks")
                    and hasattr(a, "patch")), None)
        if rom is None:
            return original(*args, **kwargs)
        generators = _generators(args, kwargs)
        before = [_rng_state(r) for r in generators]
        scrambled = self._scrambled(rom)
        first = len(trace._events)
        result = original(*args, **kwargs)
        events = trace._events[first:]
        if not any(o < 0 for e in events for o in (e[-1] or ())):
            return result
        self.calls += 1
        after = [_rng_state(r) for r in generators]
        lines = len(self.tracer.lines) if self.tracer is not None else None
        try:
            for r, state in zip(generators, before):
                _set_rng_state(r, state)
            memo = {id(rom): scrambled}
            memo.update((id(r), r) for r in generators)
            copy_args = [scrambled if a is rom else _copied(a, memo) for a in args]
            copy_kwargs = {k: scrambled if v is rom else _copied(v, memo)
                           for k, v in kwargs.items()}
            original(*copy_args, **copy_kwargs)
            ok = True
        except Exception as e:
            ok = False
            self.failed += 1
            self.errors.append((getattr(original, "__qualname__", "?"), type(e).__name__,
                                str(e)[:200]))
        finally:
            for r, state in zip(generators, after):
                _set_rng_state(r, state)
            if lines is not None:
                del self.tracer.lines[lines:]
        for n, event in enumerate(events, first):
            if not isinstance(event[-2], int) and len(event[-2]):
                self.flags[n] = (self._event_flags(event, scrambled) if ok
                                 else (False,) * len(event[-2]))
        return result

    def _event_flags(self, event, scrambled):
        if event[0] == "B":
            offset, values = event[3], event[4]
            out = []
            for i, v in enumerate(values):
                nr, addr = self._where(offset + i)
                bank = scrambled.banks[nr]
                out.append(addr < len(bank) and bank[addr] == v)
            return tuple(out)
        _, _, _, table, key, value, _ = event
        if isinstance(value, int):
            return ()
        try:
            other = getattr(scrambled, table)[key]
        except (AttributeError, IndexError, KeyError):
            other = None
        if isinstance(other, int) or other is None or len(other) != len(value):
            return (False,) * len(value)
        return tuple(a == b for a, b in zip(value, bytes(memoryview(other))))

    def _where(self, offset):
        bases = self.trace._bases
        nr = max(n for n, base in enumerate(bases) if base <= offset)
        return nr, offset - bases[nr]

    def known(self):
        if self._known is None:
            source = self.trace._source
            self._known = self._known_of(source)
            self._scrambled_source = bytes(
                b if p in self._known else b ^ _mask(p) for p, b in enumerate(source))
        return self._known

    def _scrambled(self, rom):
        trace = self.trace
        known = self.known()
        state = {}
        for name, value in vars(rom).items():
            table = trace._table_names.get(id(value))
            if name == "banks":
                state[name] = [self._bank(n, b) for n, b in enumerate(value)]
            elif table is not None and table[1] is value:
                state[name] = self._table(table[0], value, known)
            else:
                state[name] = copy.deepcopy(value)
        out = object.__new__(type(rom))
        out.__dict__.update(state)
        out.patch = types.MethodType(_unchecked_patch(type(rom).patch), out)
        return out

    def _bank(self, nr, bank):
        trace = self.trace
        base = getattr(bank, "_base", None)
        if base is None:
            return bytearray(bank)
        out = bytearray(self._scrambled_source[base:base + len(bank)])
        if len(out) != len(bank):
            out = bytearray(b ^ _mask(base + i) for i, b in enumerate(bytes(memoryview(bank))))
        written = trace._masks.get(nr)
        if written is None:
            return out
        values, origins = bytes(memoryview(bank)), bank._orig
        for m in re.finditer(rb"[^\x00]+", bytes(written)):
            for i in range(m.start(), min(m.end(), len(values))):
                out[i] = values[i] if origins[i] < 0 else values[i] ^ _mask(base + i)
        return out

    def _table(self, name, table, known):
        written = self.trace._entries.get(name, {})
        cls = type(table).__mro__[1]
        out = cls.__new__(cls)

        def entry(key, value):
            if isinstance(value, int):
                return int(value)
            if not isinstance(value, (bytes, bytearray)):
                return copy.deepcopy(value)
            data = bytes(memoryview(value))
            origins = list(getattr(value, "origins", None) or [KNOWN] * len(data))
            was_written = key in written
            scrambled = bytearray(
                v if (q < 0 and was_written) or (q >= 0 and not was_written and q in known)
                else v ^ _mask(q if q >= 0 else i)
                for i, (v, q) in enumerate(zip(data, origins)))
            return scrambled if isinstance(value, bytearray) else bytes(scrambled)

        for attr, value in vars(table).items():
            if isinstance(value, list) and any(isinstance(v, (bytes, bytearray)) for v in value):
                value = [entry("0x%04X" % n, v) for n, v in enumerate(value)]
            elif isinstance(value, dict) and any(isinstance(v, (bytes, bytearray))
                                                 for v in value.values()):
                value = {k: entry(k, v) for k, v in value.items()}
            else:
                value = copy.deepcopy(value)
            out.__dict__[attr] = value
        return out


def _copied(value, memo):
    try:
        return copy.deepcopy(value, memo)
    except Exception:
        return value


def _unchecked_patch(patch):
    # LADXR's patch takes hex text and decodes it with its module's h2b; a stand-in takes bytes.
    decode = patch.__globals__.get("h2b", bytes)

    def unchecked(self, bank_nr, addr, old, new, *args, **kwargs):
        if isinstance(old, (str, bytes, bytearray)):
            old_bytes = decode(old)
            self.banks[bank_nr][addr:addr + len(old_bytes)] = old_bytes
        return patch(self, bank_nr, addr, old, new, *args, **kwargs)
    return unchecked


def _generators(args, kwargs):
    found = [a for a in list(args) + list(kwargs.values()) if isinstance(a, _RANDOM)]
    glob = getattr(random.random, "__self__", None)
    if isinstance(glob, _RANDOM) and all(glob is not r for r in found):
        found.append(glob)
    return found


def _rng_state(r):
    return _RANDOM.getstate(r), dict(vars(r))


def _set_rng_state(r, state):
    _RANDOM.setstate(r, state[0])
    vars(r).clear()
    vars(r).update(state[1])


def install(trace, targets, sides, known=None, tracer=None):
    if known is None:
        import known_source
        known = known_source.build
    rerun = Rerun(trace, sides, known, tracer)
    for owner, name in targets:
        rerun.wrap(owner, name)
    return rerun
