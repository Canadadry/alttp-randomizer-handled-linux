<?php
// Usage: php prng_check.php <vt_dir>
// Exit 0 when the patched VT PRNG matches every vector of prng_vectors.txt, 1 otherwise.

if ($argc !== 2) {
    fwrite(STDERR, "usage: php prng_check.php <vt_dir>\n");
    exit(2);
}
require $argv[1] . '/app/Helpers/number.php';
require $argv[1] . '/app/Helpers/array.php';

function fail(string $what, array $expected, array $actual): void
{
    fwrite(STDERR, "prng: MISMATCH $what\n  expected: " . implode(' ', $expected)
        . "\n  actual:   " . implode(' ', $actual) . "\n");
    exit(1);
}

$checked = 0;
foreach (file(__DIR__ . '/prng_vectors.txt', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $line) {
    if ($line[0] === '#') {
        continue;
    }
    $f = explode(' ', $line);
    $n = array_map('intval', array_slice($f, 1));
    switch ($f[0]) {
        case 'splitmix32':
            $actual = [prng_splitmix32($n[0])];
            $expected = [$n[1]];
            break;
        case 'next':
            prng_seed($n[0]);
            $expected = array_slice($n, 1);
            $actual = array_map(fn () => prng_next(), $expected);
            break;
        case 'range':
            prng_seed($n[0]);
            $expected = array_slice($n, 3);
            $actual = array_map(fn () => get_random_int($n[1], $n[2]), $expected);
            break;
        case 'fy_shuffle':
            prng_seed($n[0]);
            $expected = array_slice($n, 2);
            $actual = fy_shuffle($n[1] > 0 ? range(0, $n[1] - 1) : []);
            break;
        default:
            fwrite(STDERR, "prng: unknown vector line: $line\n");
            exit(1);
    }
    if ($actual !== $expected) {
        fail($line, $expected, $actual);
    }
    $checked++;
}

foreach ([[1, 0], [0, 0x100000000], [-1, 0xFFFFFFFF]] as [$min, $max]) {
    try {
        get_random_int($min, $max);
    } catch (\Error $e) {
        $checked++;
        continue;
    }
    fail("get_random_int($min, $max) must throw an Error", ['Error'], ['no throw']);
}

echo "prng: $checked known answers ok\n";
