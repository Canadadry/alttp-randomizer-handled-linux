<?php
// Usage: php gen_case.php <vt_dir> <case_dir> <out_dir>
// Writes spoiler.json, trace.txt, attempts, rom_writes.txt and rom_buffers.txt into <out_dir> only on success;
// exit 1 otherwise.

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
require __DIR__ . '/oracle_rom.php';
oracle_check_sram_overrides(fn ($why) => die_case($case, $why));

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

/** The only write tagged $tag, as a byte list; dies unless there is exactly one. */
function oracle_single_write(string $case, OracleRom $rom, string $tag): array
{
    $found = array_values(array_filter($rom->writes, fn ($w) => $w[2] === $tag));
    if (count($found) !== 1) {
        die_case($case, count($found) . " $tag writes");
    }
    return array_values(unpack('C*', $found[0][1]));
}

function oracle_unhex(string $hex): array
{
    return $hex === '-' ? [] : array_values(unpack('C*', hex2bin($hex)));
}

/** Dies unless the tagged writes are VT's write log and the buffer lines rebuild the text and SRAM writes. */
function oracle_check_writes(string $case, OracleRom $rom, array $buffers): void
{
    $vt_log = $rom->getWriteLog();
    if (count($vt_log) !== count($rom->writes)) {
        die_case($case, count($rom->writes) . ' tagged writes, VT logged ' . count($vt_log));
    }
    foreach ($vt_log as $i => $entry) {
        if ($entry !== [$rom->writes[$i][0] => array_values(unpack('C*', $rom->writes[$i][1]))]) {
            die_case($case, "tagged write $i differs from VT's write log");
        }
    }

    $table = array_map(fn ($line) => explode(' ', $line, 2), oracle_text_lines(oracle_default_text()));
    $table = array_combine(array_column($table, 0), array_map('oracle_unhex', array_column($table, 1)));
    $sram = (new OracleInitialSram)->bytes();
    foreach ($buffers as $line) {
        $f = explode(' ', $line);
        if ($f[0] === 'text') {
            $table[$f[1]] = oracle_unhex($f[2]);
        } elseif ($f[0] === 'sram' && $f[2] !== '-') {
            foreach (explode(',', $f[2]) as $change) {
                [$k, $v] = explode('=', $change);
                $sram[hexdec($k)] = hexdec($v);
            }
        }
    }
    if (array_merge(...array_values($table)) !== oracle_single_write($case, $rom, 'writeText')) {
        die_case($case, 'the text lines do not rebuild the writeText write');
    }
    ksort($sram);
    if (array_values($sram) !== oracle_single_write($case, $rom, 'writeInitialSram')) {
        die_case($case, 'the sram lines do not rebuild the writeInitialSram write');
    }
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

// prepSeed runs writeToRom before isWinnable, which draws nothing: the order changes no draw.
$drawn = count(prng_trace());
try {
    $rom = new OracleRom;
    $world->writeToRom($rom, false);
    $rom->label = 'setHeartBeepSpeed';
    $rom->setHeartBeepSpeed($config['heart_beep']);
    $rom->label = null;
} catch (\Throwable $e) {
    die_case($case, 'writeToRom: ' . get_class($e) . ': ' . $e->getMessage() . ' at ' . $e->getFile() . ':' . $e->getLine());
}
array_push($trace, ...array_slice(prng_trace(), $drawn));
$buffers = OracleBufferLog::$lines;
oracle_check_writes($case, $rom, $buffers);

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
file_put_contents("$out_dir/rom_writes.txt", implode("\n", array_map(
    fn ($w) => sprintf('%06X %s %s', $w[0], strtoupper(bin2hex($w[1])), $w[2]),
    $rom->writes
)) . "\n");
file_put_contents("$out_dir/rom_buffers.txt", $buffers === [] ? '' : implode("\n", $buffers) . "\n");
if (realpath($out_dir) !== realpath($case_dir)) {
    copy("$case_dir/config.json", "$out_dir/config.json");
    copy("$case_dir/seed", "$out_dir/seed");
}
echo "gen_case: $case: attempts $attempt, " . count($trace) . ' draws, ' . count($rom->writes) . ' writes, '
    . count($buffers) . " buffer calls\n";
