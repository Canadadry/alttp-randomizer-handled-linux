<?php
// Usage: php credit_tables.php <vt_dir> <out_dir>
// Writes <out_dir>/credit_tables.txt, each <hex> a scene's segment of Credits::getBinaryData()['data']
// (its pointer to the next) on a fresh Credits after one updateCreditLine(<scene>, 0, <text>):
// "default <scene> <hex>" per scene untouched, in Credits order;
// "pick <scene> <index> <hex>" per randomizeCredits alternative, in source order;
// "item <scene> <raw name> <hex>" per override scene (Credits order) and item of Item::all (its order),
// the text that scene's location class's getItemCreditsText gives with the item set on it.

if ($argc !== 3) {
    fwrite(STDERR, "usage: php credit_tables.php <vt_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;
require __DIR__ . '/oracle_case.php';
oracle_boot_vt($vt_dir);

function credit_tables_die(string $why): never
{
    fwrite(STDERR, "credit_tables: $why\n");
    exit(1);
}

$scene_names = array_keys((new ReflectionProperty(ALttP\Support\Credits::class, 'scenes'))
    ->getValue(new ALttP\Support\Credits));

function credit_segment(array $scene_names, string $scene, ?string $text): string
{
    $credits = new ALttP\Support\Credits;
    if ($text !== null && $credits->updateCreditLine($scene, 0, $text) === false) {
        credit_tables_die("updateCreditLine refuses $scene");
    }
    $binary = $credits->getBinaryData();
    $i = array_search($scene, $scene_names, true);
    $from = $binary['pointers'][$i];
    $bytes = array_slice($binary['data'], $from, $binary['pointers'][$i + 1] - $from);
    return bin2hex(pack('C*', ...$bytes));
}

$lines = [];
foreach ($scene_names as $scene) {
    $lines[] = "default $scene " . credit_segment($scene_names, $scene, null);
}

// randomizeCredits is read from its source: its lists live inside the method, behind draws.
$method = new ReflectionMethod(ALttP\Randomizer::class, 'randomizeCredits');
$source = implode('', array_slice(file($method->getFileName()),
    $method->getStartLine() - 1, $method->getEndLine() - $method->getStartLine() + 1));
$list_re = '/fy_shuffle\(\[(.*?)\]\)/s';
preg_match_all($list_re, $source, $lists, PREG_OFFSET_CAPTURE);
$used = [];
$alternatives = function (int $offset) use ($lists, &$used): array {
    foreach ($lists[0] as $k => [, $at]) {
        if ($at === $offset) {
            $used[$k] = true;
            return eval('return [' . $lists[1][$k][0] . '];');
        }
    }
    credit_tables_die("no list at $offset");
};
preg_match_all('/setCredit\(\'(\w+)\',\s*(.*?)\);\n/s', $source, $calls, PREG_SET_ORDER | PREG_OFFSET_CAPTURE);
if (count($calls) === 0) {
    credit_tables_die('no setCredit in randomizeCredits');
}
foreach ($calls as $call) {
    $scene = $call[1][0];
    [$arg, $arg_at] = $call[2];
    if (!in_array($scene, $scene_names, true)) {
        credit_tables_die("unknown scene $scene");
    }
    if (preg_match($list_re, $arg, $m, PREG_OFFSET_CAPTURE)) {
        $texts = $alternatives($arg_at + $m[0][1]);
    } elseif (preg_match('/^"[^"]*\$(\w+)[^"]*"$/', $arg, $m)) {
        $var = $m[1];
        if (!preg_match('/\$' . $var . ' = Arr::first\((fy_shuffle\(\[)/', $source, $assign, PREG_OFFSET_CAPTURE)) {
            credit_tables_die("no list assigned to \$$var");
        }
        $texts = [];
        foreach ($alternatives($assign[1][1]) as $value) {
            $texts[] = (static function (string $var, string $value, string $arg): string {
                $$var = $value;
                return eval("return $arg;");
            })($var, $value, $arg);
        }
    } elseif (preg_match('/^"[^"$\\\\]*"$/', $arg)) {
        $texts = [substr($arg, 1, -1)];
    } else {
        credit_tables_die("unparsed setCredit('$scene', $arg)");
    }
    foreach ($texts as $index => $text) {
        $lines[] = "pick $scene $index " . credit_segment($scene_names, $scene, $text);
    }
}
if (count($used) !== count($lists[0])) {
    credit_tables_die('a fy_shuffle list feeds no setCredit');
}

$override_classes = [
    'house' => ALttP\Location\Npc\Uncle::class,
    'zora' => ALttP\Location\Npc\Zora::class,
    'witch' => ALttP\Location\Npc\Witch::class,
    'grove' => ALttP\Location\Dig\HauntedGrove::class,
    'kakariko2' => ALttP\Location\Npc\BugCatchingKid::class,
    'pedestal' => ALttP\Location\Pedestal::class,
];
ALttP\Item::clearCache();
$world = ALttP\World::factory('standard', []);
$items = ALttP\Item::all($world);
foreach ($scene_names as $scene) {
    if (!isset($override_classes[$scene])) {
        continue;
    }
    $class = $override_classes[$scene];
    $class_source = (string) file_get_contents((new ReflectionClass($class))->getFileName());
    if (!str_contains($class_source, "setCredit('$scene', \$this->getItemCreditsText())")) {
        credit_tables_die("$class does not set the $scene credit");
    }
    $found = $world->getLocations()->filter(function ($location) use ($class) {
        return get_class($location) === $class;
    })->values();
    if (count($found) !== 1) {
        credit_tables_die(count($found) . " locations of class $class");
    }
    $location = $found[0];
    $credits_text = Closure::bind(function () {
        return $this->getItemCreditsText();
    }, $location, $class);
    foreach ($items as $item) {
        try {
            $location->setItem($item);
        } catch (Throwable $e) {
            continue;
        }
        $lines[] = "item $scene {$item->getRawName()} " . credit_segment($scene_names, $scene, $credits_text());
    }
}

if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    credit_tables_die("cannot create $out_dir");
}
file_put_contents("$out_dir/credit_tables.txt", implode("\n", $lines) . "\n");
echo 'credit_tables: ' . count($lines) . " lines\n";
