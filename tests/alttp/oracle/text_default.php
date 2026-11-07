<?php
// Usage: php text_default.php <vt_dir> <out_dir>
// Writes <out_dir>/text_default.txt: "<id> <hex>" per text id, in table order, as VT's Rom constructor leaves it.

if ($argc !== 3) {
    fwrite(STDERR, "usage: php text_default.php <vt_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
oracle_boot_vt($vt_dir);
require __DIR__ . '/oracle_rom.php';

$lines = oracle_text_lines(oracle_default_text());
if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    fwrite(STDERR, "text_default: cannot create $out_dir\n");
    exit(1);
}
file_put_contents("$out_dir/text_default.txt", implode("\n", $lines) . "\n");
echo 'text_default: ' . count($lines) . " ids\n";
