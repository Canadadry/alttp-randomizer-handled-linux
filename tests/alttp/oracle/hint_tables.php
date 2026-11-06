<?php
// Usage: php hint_tables.php <vt_dir> <out_dir>
// Writes <out_dir>/hint_tables.txt: "item <raw name> <n>" and "location <name> <n>" per key of
// resources/lang/en/hint.php, in file order, n the alternatives __() returns (0 for a plain string),
// then "strings <file> <count>" per strings/*.txt, in name order, count the entries getTextArray keeps.

if ($argc !== 3) {
    fwrite(STDERR, "usage: php hint_tables.php <vt_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
oracle_boot_vt($vt_dir);

$lines = [];
$table = require "$vt_dir/resources/lang/en/hint.php";
foreach (['item', 'location'] as $kind) {
    foreach (array_keys($table[$kind]) as $name) {
        $value = __("hint.$kind.$name");
        if (!is_array($value) && $value === "hint.$kind.$name") {
            fwrite(STDERR, "hint_tables: __() does not resolve hint.$kind.$name\n");
            exit(1);
        }
        $lines[] = "$kind $name " . (is_array($value) ? count($value) : 0);
    }
}

// getTextArray reads no state of the Randomizer, so no world is built for it.
$randomizer = (new ReflectionClass(ALttP\Randomizer::class))->newInstanceWithoutConstructor();
$files = glob("$vt_dir/strings/*.txt");
sort($files, SORT_STRING);
foreach ($files as $path) {
    $file = basename($path);
    $lines[] = "strings $file " . count($randomizer->getTextArray("strings/$file"));
}

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    fwrite(STDERR, "hint_tables: cannot create $out_dir\n");
    exit(1);
}
file_put_contents("$out_dir/hint_tables.txt", implode("\n", $lines) . "\n");
echo 'hint_tables: ' . count($lines) . " lines\n";
