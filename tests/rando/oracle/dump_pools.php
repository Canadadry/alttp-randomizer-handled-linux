<?php
// Usage: php dump_pools.php <vt_dir> <out_dir> <case_dir>...
// Writes pool_lists.txt and prepared_pools.txt into <out_dir>. Exit 1, writing nothing, when a case's
// draws are not the first lines of its trace.txt.

const GETTERS = [
    'dungeon' => 'getDungeonPool',
    'advancement' => 'getAdvancementItems',
    'nice' => 'getNiceItems',
    'items' => 'getItemPool',
];
const RANDOMIZER_LISTS = [
    'dungeon' => 'dungeon_items',
    'advancement' => 'advancement_items',
    'nice' => 'nice_items',
    'items' => 'trash_items',
];

if ($argc < 4) {
    fwrite(STDERR, "usage: php dump_pools.php <vt_dir> <out_dir> <case_dir>...\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
$case_dirs = array_slice($argv, 3);
$cases = array_map('oracle_load_case', $case_dirs);
oracle_boot_vt($vt_dir);

$out = [
    '# VT\'s four item lists at attempt 0 of every case, written by dump_pools.php (run by gen_golden.sh),',
    '# which first checks its draws against the case\'s golden trace. Checked against the C pools by',
    '# test_generate --fixtures.',
    '# case <name> <seed> <key>=<value> for the 11 config.json keys',
    '# <dungeon|advancement|nice|items> <raw item names in VT\'s order>, in prepareWorld\'s getter order',
];
$prepared_out = [
    '# VT\'s four item lists as Randomizer::prepareWorld leaves them at attempt 0 of every case, final order,',
    '# written by dump_pools.php (run by gen_golden.sh), which first checks its draws against the case\'s',
    '# golden trace up to the fill. Checked against the C pools by test_generate --fixtures.',
    '# case <name> <seed> <key>=<value> for the 11 config.json keys',
    '# <dungeon|advancement|nice|items> <raw item names in the Randomizer\'s order>',
];
$records = 0;
foreach ($cases as $i => $case) {
    try {
        $world = oracle_attempt_world($case, 0);
        $lists = [];
        foreach (GETTERS as $list => $getter) {
            prng_trace_label($getter);
            $lists[$list] = array_map(fn ($item) => $item->getRawName(), $world->$getter());
            prng_trace_label(null);
        }
    } catch (\Throwable $e) {
        die_case($case['name'], get_class($e) . ': ' . $e->getMessage() . ' at ' . $e->getFile() . ':' . $e->getLine());
    }

    $golden = @file("{$case_dirs[$i]}/trace.txt", FILE_IGNORE_NEW_LINES);
    if ($golden === false) {
        die_case($case['name'], 'trace.txt is missing');
    }
    check_draws($case['name'], $golden, prng_trace(),
        fn ($next) => $next !== 'crystals' && !in_array($next, GETTERS, true));

    try {
        $world = oracle_attempt_world($case, 0);
        $randomizer = new ALttP\Randomizer([$world]);
        $randomizer->prepareWorld($world);
        $prepared = [];
        foreach (RANDOMIZER_LISTS as $list => $property) {
            $items = (new ReflectionProperty(ALttP\Randomizer::class, $property))->getValue($randomizer);
            $prepared[$list] = array_map(fn ($item) => $item->getRawName(), $items);
        }
    } catch (\Throwable $e) {
        die_case($case['name'], 'prepareWorld: ' . get_class($e) . ': ' . $e->getMessage() . ' at ' . $e->getFile() . ':' . $e->getLine());
    }
    check_draws($case['name'], $golden, prng_trace(), fn ($next) => $next === 'fill');

    $options = array_map(fn ($key) => "$key={$case['config'][$key]}", CONFIG_KEYS);
    $case_line = implode(' ', ['case', $case['name'], $case['seed'], ...$options]);
    $out[] = $case_line;
    $prepared_out[] = $case_line;
    foreach ($lists as $list => $names) {
        $out[] = implode(' ', [$list, ...$names]);
        $prepared_out[] = implode(' ', [$list, ...$prepared[$list]]);
        $records++;
    }
}

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    fwrite(STDERR, "dump_pools: cannot create $out_dir\n");
    exit(1);
}
file_put_contents("$out_dir/pool_lists.txt", implode("\n", $out) . "\n");
file_put_contents("$out_dir/prepared_pools.txt", implode("\n", $prepared_out) . "\n");
echo 'dump_pools: ' . count($cases) . " cases, $records lists in each fixture\n";

/** Dies unless $draws are the first lines of $golden and $next_ok accepts the stage of the line after them. */
function check_draws(string $case, array $golden, array $draws, callable $next_ok): void
{
    foreach ($draws as $n => $line) {
        if (($golden[$n] ?? null) !== $line) {
            die_case($case, "draw " . ($n + 1) . ": dump has '$line', golden has '" . ($golden[$n] ?? 'nothing') . "'");
        }
    }
    $next_stage = explode(' ', $golden[count($draws)] ?? '0 0 -')[2];
    if (!$next_ok($next_stage)) {
        die_case($case, 'draw ' . (count($draws) + 1) . ": golden has '" . ($golden[count($draws)] ?? 'nothing') . "', dump has nothing");
    }
}
