<?php
// Usage: php gen_case.php <vt_dir> <case_dir> <out_dir>
// Writes spoiler.json, trace.txt and attempts into <out_dir> only on success; exit 1 otherwise.

const WATCHDOG = 1000;

if ($argc !== 4) {
    fwrite(STDERR, "usage: php gen_case.php <vt_dir> <case_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $case_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
$loaded = oracle_load_case($case_dir);
$case = $loaded['name'];
$config = $loaded['config'];
oracle_boot_vt($vt_dir);

function normalize(array $a): array
{
    foreach ($a as $k => $v) {
        if (is_array($v)) {
            $a[$k] = normalize($v);
        }
    }
    if (!array_is_list($a)) {
        ksort($a, SORT_STRING);
    }
    return $a;
}

$trace = [];
for ($attempt = 0; $attempt < WATCHDOG; ++$attempt) {
    try {
        $world = oracle_attempt_world($loaded, $attempt);
        $rand = new ALttP\Randomizer([$world]);
        // Only randomize() and isWinnable() may fail an attempt: an exception elsewhere is a harness bug.
        try {
            $rand->randomize();
            $won = (new ALttP\Support\WorldCollection($rand->getWorlds()))->isWinnable();
        } catch (\Exception $e) {
            $won = false;
        }
    } catch (\Throwable $e) {
        die_case($case, "attempt $attempt: " . get_class($e) . ': ' . $e->getMessage()
            . ' at ' . $e->getFile() . ':' . $e->getLine());
    }
    array_push($trace, ...prng_trace());
    if ($won) {
        break;
    }
}
if ($attempt === WATCHDOG) {
    die_case($case, 'no winnable attempt within ' . WATCHDOG . ' attempts');
}

$spoiler = $world->getSpoiler([
    'entry_crystals_ganon' => $config['crystals_ganon'],
    'entry_crystals_tower' => $config['crystals_tower'],
    'worlds' => 1,
]);
unset($spoiler['meta']['build']);
$json = json_encode(normalize($spoiler), JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE
    | JSON_THROW_ON_ERROR);

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    die_case($case, "cannot create $out_dir");
}
file_put_contents("$out_dir/spoiler.json", "$json\n");
file_put_contents("$out_dir/trace.txt", $trace === [] ? '' : implode("\n", $trace) . "\n");
file_put_contents("$out_dir/attempts", "$attempt\n");
if (realpath($out_dir) !== realpath($case_dir)) {
    copy("$case_dir/config.json", "$out_dir/config.json");
    copy("$case_dir/seed", "$out_dir/seed");
}
echo "gen_case: $case: attempts $attempt, " . count($trace) . " draws\n";
