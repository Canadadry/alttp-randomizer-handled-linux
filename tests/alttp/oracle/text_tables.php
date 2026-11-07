<?php
// Usage: php text_tables.php <vt_dir> <out_dir>
// Writes <out_dir>/text_tables.txt, each <HEX> a text id's bytes after one setString on a fresh default table:
// "pick <id> <file> <index> <HEX>" per alternative of each string file setTexts or HintService draws from,
// in getTextArray order (end_triforce with its "{NOBORDER}\n" prefix);
// "item <id> <raw name> <HEX>" per tablet and pedestal id and item of Item::all (its order), the text
// that id's location class gives with the item set on it ("rom.genericKeys" off);
// "hint_item <raw name> <index> <HEX>" and "hint_location <index> <HEX> <name>" per alternative of
// resources/lang/en/hint.php, in file order, <HEX> the raw UTF-8 name, index 0 for a plain string.

if ($argc !== 3) {
    fwrite(STDERR, "usage: php text_tables.php <vt_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
oracle_boot_vt($vt_dir);
require __DIR__ . '/oracle_rom.php';

function text_tables_die(string $why): never
{
    fwrite(STDERR, "text_tables: $why\n");
    exit(1);
}

$default_text = oracle_default_text();

function text_bytes(OracleText $default_text, string $id, string $value): string
{
    $text = clone $default_text;
    $text->setString($id, $value);
    return oracle_hex($text->table()[$id]);
}

function source_of(string $class, string $method): string
{
    $m = new ReflectionMethod($class, $method);
    return implode('', array_slice(file($m->getFileName()),
        $m->getStartLine() - 1, $m->getEndLine() - $m->getStartLine() + 1));
}

$lines = [];

// Each file's id is checked against the source that draws from it, so a renamed id fails here.
$randomizer = (new ReflectionClass(ALttP\Randomizer::class))->newInstanceWithoutConstructor();
$set_texts = source_of(ALttP\Randomizer::class, 'setTexts');
$hint_source = (string) file_get_contents((new ReflectionClass(ALttP\Services\HintService::class))->getFileName());
$string_files = [
    'uncle' => ['uncle_leaving_text', ''],
    'blind' => ['blind_by_the_light', ''],
    'tavern_man' => ['kakariko_tavern_fisherman', ''],
    'ganon_1' => ['ganon_fall_in', ''],
    'ganon_phase_3_no_silvers' => ['ganon_phase_3_no_silvers', ''],
    'triforce' => ['end_triforce', "{NOBORDER}\n"],
    'hint' => ['telepathic_tile_eastern_palace', ''],
];
foreach ($string_files as $file => [$id, $prefix]) {
    if ($file === 'hint') {
        if (!str_contains($hint_source, "base_path('strings/hint.txt')") || !str_contains($hint_source, "'$id'")) {
            text_tables_die('HintService does not draw strings/hint.txt onto its tiles');
        }
        $alternatives = (new ReflectionProperty(ALttP\Services\HintService::class, 'joke_hints'))
            ->getValue(new ALttP\Services\HintService([], []));
    } else {
        if (!str_contains($set_texts, "'$file' => \$this->getTextArray('strings/$file.txt')")) {
            text_tables_die("setTexts does not read strings/$file.txt");
        }
        $draw = "Arr::first(fy_shuffle(\$strings['$file']))";
        $call = $file === 'ganon_phase_3_no_silvers'
            ? "\$fake_silvers_hint = $draw;"
            : "setText('$id', " . ($prefix === '' ? '' : '"{NOBORDER}\\n" . ') . "$draw)";
        if (!str_contains($set_texts, $call)
            || ($file === 'ganon_phase_3_no_silvers' && !str_contains($set_texts, "setText('$id', \$fake_silvers_hint)"))) {
            text_tables_die("setTexts does not draw $file onto $id");
        }
        $alternatives = $randomizer->getTextArray("strings/$file.txt");
    }
    if (count($alternatives) === 0) {
        text_tables_die("strings/$file.txt has no entry");
    }
    foreach (array_values($alternatives) as $index => $value) {
        $lines[] = "pick $id $file $index " . text_bytes($default_text, $id, $prefix . $value);
    }
}

$item_classes = [
    'tablet_ether_book' => [ALttP\Location\Drop\Ether::class, 'getItemText'],
    'tablet_bombos_book' => [ALttP\Location\Drop\Bombos::class, 'getItemText'],
    'mastersword_pedestal_translated' => [ALttP\Location\Pedestal::class, 'getItemPedestalText'],
];
ALttP\Item::clearCache();
$world = ALttP\World::factory('standard', []);
if ($world->config('rom.genericKeys', false)) {
    text_tables_die('rom.genericKeys is on by default');
}
$items = ALttP\Item::all($world);
foreach ($item_classes as $id => [$class, $method]) {
    if (!str_contains(source_of($class, 'writeItem'), "setText('$id', \$this->$method())")) {
        text_tables_die("$class does not set $id");
    }
    $found = $world->getLocations()->filter(function ($location) use ($class) {
        return get_class($location) === $class;
    })->values();
    if (count($found) !== 1) {
        text_tables_die(count($found) . " locations of class $class");
    }
    $location = $found[0];
    $item_text = Closure::bind(function () use ($method) {
        return $this->$method();
    }, $location, $class);
    foreach ($items as $item) {
        try {
            $location->setItem($item);
        } catch (Throwable $e) {
            continue;
        }
        $lines[] = "item $id {$item->getRawName()} " . text_bytes($default_text, $id, $item_text());
    }
}

$table = require "$vt_dir/resources/lang/en/hint.php";
foreach (['item', 'location'] as $kind) {
    foreach (array_keys($table[$kind]) as $name) {
        $value = __("hint.$kind.$name");
        if (!is_array($value) && $value === "hint.$kind.$name") {
            continue;
        }
        foreach (array_values(is_array($value) ? $value : [$value]) as $index => $alternative) {
            $hex = strtoupper(bin2hex($alternative));
            $lines[] = $kind === 'item' ? "hint_item $name $index $hex" : "hint_location $index $hex $name";
        }
    }
}

foreach ($lines as $line) {
    if (preg_match('/\s$|[\n\r]|  /', $line)) {
        text_tables_die("unreadable line: $line");
    }
}
if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    text_tables_die("cannot create $out_dir");
}
file_put_contents("$out_dir/text_tables.txt", implode("\n", $lines) . "\n");
echo 'text_tables: ' . count($lines) . " lines\n";
