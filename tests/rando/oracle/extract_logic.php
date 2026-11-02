<?php
// Usage: php extract_logic.php <vt_dir> <out_dir>
// Writes world_dump.txt, predicate_vectors.txt and one row file per ROW_TREES entry into <out_dir>.
// Exit 1, writing nothing, on an unknown method in a ROW_TREES tree or a row its record does not replay.

use ALttP\Boss;
use ALttP\Item;
use ALttP\Location;
use ALttP\Randomizer;
use ALttP\Region;
use ALttP\Support\ItemCollection;
use ALttP\World;

if ($argc !== 3) {
    fwrite(STDERR, "usage: php extract_logic.php <vt_dir> <out_dir>\n");
    exit(2);
}
[, $vt_dir, $out_dir] = $argv;

function fail(string $why): never
{
    fwrite(STDERR, "extract_logic: $why\n");
    exit(1);
}

ini_set('memory_limit', '1024M');
require "$vt_dir/vendor/autoload.php";
$app = require "$vt_dir/bootstrap/app.php";
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
config([
    'logging.channels.oracle' => ['driver' => 'monolog', 'handler' => Monolog\Handler\NullHandler::class],
    'logging.default' => 'oracle',
]);
// Same Seed pre-boot as gen_case.php: without bcmath or gmp, World::factory() throws until Seed is booted.
if (!extension_loaded('bcmath') && !extension_loaded('gmp')) {
    try {
        new ALttP\Seed;
    } catch (RuntimeException $e) {
    }
}

const TEST_CONFIG = ['difficulty' => 'test_rules', 'logic' => 'NoGlitches'];
// VT test tree under tests/ => its row fixture.
const ROW_TREES = [
    'NoGlitches' => 'noglitches_rows.txt',
    'OverworldGlitches' => 'overworld_glitches_rows.txt',
    'MajorGlitches' => 'major_glitches_rows.txt',
    'Inverted' => 'inverted_rows.txt',
    'InvertedOverworldGlitches' => 'inverted_overworld_glitches_rows.txt',
    'InvertedMajorGlitches' => 'inverted_major_glitches_rows.txt',
];
const VARIANTS = ['standard', 'open', 'inverted', 'retro'];
// Every key read by the ported files: Region/Standard, Region/Open, Region/Inverted, Region, Location*, Boss,
// ItemCollection.
const CONFIG_KEYS = [
    'accessibility', 'canBootsClip', 'canBunnyRevive', 'canBunnySurf', 'canDungeonRevive', 'canFakeFlipper',
    'canMirrorClip', 'canMirrorWrap', 'canOWYBA', 'canOneFrameClipOW', 'canOneFrameClipUW', 'canSuperBunny',
    'canSuperSpeed', 'canTransitionWrapped', 'canWaterWalk', 'cantTakeDamage', 'crystals.ganon', 'crystals.tower', 'dungeonItems',
    'enemizer.enemyHealth', 'goal', 'ignoreCanKillEscapeThings', 'item.Goal.Required',
    'item.overflow.count.Shield', 'item.require.Lamp', 'itemPlacement', 'logic', 'mode.state', 'mode.weapons',
    'region.bossNormalLocation', 'region.cantTakeDamage', 'region.forceSkullWoodsKey',
    'region.requireBetterBow', 'region.requireBetterSword', 'region.swordsInPool', 'region.wildBigKeys',
    'region.wildCompasses', 'region.wildKeys', 'region.wildMaps', 'rom.BottleFill.Magic',
    'rom.CatchableFairies', 'rom.genericKeys', 'rom.mapOnPickup', 'rom.rupeeBow', 'rom.vanillaBigKeys',
    'rom.vanillaCompasses', 'rom.vanillaKeys', 'rom.vanillaMaps', 'turtlerock.wastekey',
];

// The baseline case's World::factory() call in gen_case.php, its crystal counts still strings.
const BASELINE_CONFIG = [
    'itemPlacement' => 'advanced', 'dungeonItems' => 'standard', 'accessibility' => 'items', 'goal' => 'ganon',
    'crystals.ganon' => '7', 'crystals.tower' => '7', 'entrances' => 'none', 'mode.weapons' => 'randomized',
    'tournament' => false, 'spoilers' => 'on', 'allow_quickswap' => true, 'override_start_screen' => false,
    'pseudoboots' => false, 'spoil.Hints' => 'on', 'logic' => 'NoGlitches', 'item.pool' => 'normal',
    'item.functionality' => 'normal', 'enemizer.bossShuffle' => 'none', 'enemizer.enemyShuffle' => 'none',
    'enemizer.enemyDamage' => 'default', 'enemizer.enemyHealth' => 'default', 'enemizer.potShuffle' => 'off',
];

function vector_configs(): array
{
    return [
        'test' => ['standard', TEST_CONFIG],
        'baseline' => ['open', BASELINE_CONFIG],
        // expert functionality on top of the PRD's basic+swordless: BottleFill.Magic 0x20, no catchable fairies.
        'basic_swordless' => ['open', array_merge(BASELINE_CONFIG, [
            'itemPlacement' => 'basic', 'mode.weapons' => 'swordless', 'item.functionality' => 'expert',
        ])],
        'inverted' => ['inverted', TEST_CONFIG],
        'retro' => ['retro', TEST_CONFIG],
    ];
}

function reset_vt(): void
{
    Item::clearCache();
    Boss::clearCache();
    (new ReflectionProperty(World::class, 'max_world'))->setValue(null, 1);
}

function new_world(string $variant, array $config = TEST_CONFIG): World
{
    return World::factory($variant, $config);
}

function prop($object, string $class, string $name)
{
    return (new ReflectionProperty($class, $name))->getValue($object);
}

function set_prop($object, string $class, string $name, $value): void
{
    (new ReflectionProperty($class, $name))->setValue($object, $value);
}

function short_class($object): string
{
    return substr(get_class($object), strlen('ALttP\\'));
}

function raw_name(string $world_name): string
{
    return preg_replace('/:\d+$/', '', $world_name);
}

function check_name(string $name): string
{
    if (preg_match('/[\t|=\n]/', $name)) {
        fail("name with a separator character: $name");
    }
    return $name;
}

function bit(bool $b): string
{
    return $b ? '1' : '0';
}

/** @return array{0: array<string,int>, 1: array<string,int>} counts and values views, by raw name */
function views(ItemCollection $items): array
{
    $counts = [];
    foreach (prop($items, ItemCollection::class, 'item_counts') as $name => $n) {
        $counts[raw_name($name)] = $n;
    }
    $values = [];
    foreach (prop($items, ItemCollection::class, 'cached_values') as $item) {
        $values[$item->getRawName()] = ($values[$item->getRawName()] ?? 0) + 1;
    }
    return [$counts, $values];
}

function build_items(World $world, array $counts, array $values): ItemCollection
{
    $items = new ItemCollection;
    $objects = [];
    $item_counts = [];
    foreach ($counts as $name => $n) {
        $item = Item::get($name, $world);
        $objects[$item->getName()] = $item;
        $item_counts[$item->getName()] = $n;
    }
    $cached = [];
    foreach ($values as $name => $n) {
        for ($i = 0; $i < $n; $i++) {
            $cached[] = Item::get($name, $world);
        }
    }
    set_prop($items, ALttP\Support\Collection::class, 'items', $objects);
    set_prop($items, ItemCollection::class, 'item_counts', $item_counts);
    set_prop($items, ItemCollection::class, 'cached_values', $cached);
    $items->setChecksForWorld($world->id);
    return $items;
}

function item_order(): array
{
    static $order = null;
    if ($order === null) {
        reset_vt();
        $order = [];
        foreach (Item::all(new_world('standard')) as $item) {
            $order[$item->getRawName()] = count($order);
        }
    }
    return $order;
}

function all_items_base(World $world): ItemCollection
{
    return Item::all($world)->copy()->manyKeys();
}

function delta(array $view, array $base): array
{
    $d = [];
    foreach (array_keys($view + $base) as $name) {
        if (($view[$name] ?? 0) !== ($base[$name] ?? 0)) {
            $d[$name] = $view[$name] ?? 0;
        }
    }
    $order = item_order();
    uksort($d, function ($a, $b) use ($order) {
        if (!isset($order[$a], $order[$b])) {
            fail("item outside Item::all: $a $b");
        }
        return $order[$a] <=> $order[$b];
    });
    return $d;
}

function encode_view(array $view, array $base): string
{
    $from_empty = delta($view, []);
    $from_all = delta($view, $base);
    [$tag, $d] = count($from_all) < count($from_empty) ? ['A', $from_all] : ['E', $from_empty];
    if (!$d) {
        return $tag;
    }
    return $tag . ':' . implode('|', array_map(fn ($k) => check_name($k) . '=' . $d[$k], array_keys($d)));
}

function decode_view(string $s, array $base): array
{
    $view = $s[0] === 'A' ? $base : [];
    if (strlen($s) > 1) {
        foreach (explode('|', substr($s, 2)) as $entry) {
            [$name, $n] = explode('=', $entry);
            $view[$name] = (int) $n;
        }
    }
    return array_filter($view, fn ($n) => $n !== 0);
}

function encode_items(ItemCollection $items, World $world): array
{
    [$base_counts, $base_values] = views(all_items_base($world));
    [$counts, $values] = views($items);
    return [encode_view($counts, $base_counts), encode_view($values, $base_values)];
}

function decode_items(string $counts, string $values, World $world): ItemCollection
{
    [$base_counts, $base_values] = views(all_items_base($world));
    return build_items($world, decode_view($counts, $base_counts), decode_view($values, $base_values));
}

// Copied from VT's tests/TestCase.php::allItemsExcept at the pinned commit.
function all_items_except(World $world, array $remove_items): ItemCollection
{
    $items = Item::all($world)->copy()->manyKeys();
    foreach (array_merge($remove_items, ['BigKey', 'Key', 'ShopKey']) as $item) {
        switch ($item) {
            case 'AnySword':
                $items->offsetUnset('L1Sword:' . $world->id);
                $items->offsetUnset('L1SwordAndShield:' . $world->id);
                $items->offsetUnset('ProgressiveSword:' . $world->id);
                $items->offsetUnset('UncleSword:' . $world->id);
                // no break
            case 'UpgradedSword':
                $items->offsetUnset('UncleSword:' . $world->id);
                $items->offsetUnset('L2Sword:' . $world->id);
                $items->offsetUnset('MasterSword:' . $world->id);
                $items->offsetUnset('L3Sword:' . $world->id);
                $items->offsetUnset('L4Sword:' . $world->id);
                break;
            case 'AnyBottle':
                $items->offsetUnset('BottleWithBee:' . $world->id);
                $items->offsetUnset('BottleWithFairy:' . $world->id);
                $items->offsetUnset('BottleWithRedPotion:' . $world->id);
                $items->offsetUnset('BottleWithGreenPotion:' . $world->id);
                $items->offsetUnset('BottleWithBluePotion:' . $world->id);
                $items->offsetUnset('Bottle:' . $world->id);
                $items->offsetUnset('BottleWithGoldBee:' . $world->id);
                break;
            case 'AnyBow':
                $items->offsetUnset('Bow:' . $world->id);
                $items->offsetUnset('BowAndArrows:' . $world->id);
                $items->offsetUnset('BowAndSilverArrows:' . $world->id);
                $items->offsetUnset('ProgressiveBow:' . $world->id);
                break;
            case 'Flute':
                $items->offsetUnset('OcarinaActive:' . $world->id);
                $items->offsetUnset('OcarinaInactive:' . $world->id);
                break;
            case 'Gloves':
                $items->offsetUnset('ProgressiveGlove:' . $world->id);
                $items->offsetUnset('PowerGlove:' . $world->id);
                $items->offsetUnset('TitansMitt:' . $world->id);
                break;
            default:
                $items->offsetUnset($item . ':' . $world->id);
                break;
        }
    }
    return $items;
}

abstract class TestCase
{
    public $world;
    public $region;
    public $world_free_items;
    public $collected;
    public string $collection_source = 'collected';
    public $last_built = null;
    public array $captures = [];
    public $standard_world;
    public $open_world;
    public $inverted_world;
    public $retro_world;
    public $standard_randomizer;
    public $open_randomizer;
    public $inverted_randomizer;
    public $retro_randomizer;

    // VT's setUp, without the Laravel application PHPUnit's TestCase would create.
    public function setUp(): void
    {
        $this->collected = new ItemCollection;
    }

    public function tearDown(): void
    {
    }

    protected function addCollected(array $items)
    {
        foreach ($items as $item) {
            $this->collected->addItem(Item::get($item, $this->world));
        }
    }

    protected function allItems()
    {
        return $this->allItemsExcept(['BigKey', 'Key', 'ShopKey']);
    }

    protected function allItemsExcept(array $remove_items)
    {
        return $this->last_built = all_items_except($this->world, $remove_items);
    }

    // RegionTest has a region but no world; RandomizerTest has neither.
    public function worldUnderTest(): World
    {
        return $this->world ?? ($this->region ? $this->region->getWorld() : $this->standard_world);
    }

    public function assertEquals($expected, $actual, string $message = ''): void
    {
        $items = $this->collection_source === 'built' ? $this->last_built : $this->collected;
        $world = $this->worldUnderTest();
        $locations = [];
        foreach ($world->getLocations() as $location) {
            $item = $location->getItem();
            $locations[(string) $location] = $item ? $item->getRawName() : null;
        }
        $this->captures[] = [
            'expected' => $expected,
            'actual' => $actual,
            'items' => $items ? encode_items($items, $world) : ['E', 'E'],
            'locations' => $locations,
            'bosses' => region_bosses($world),
            'config' => prop($world, World::class, 'config'),
            'variant' => $world->config('mode.state'),
        ];
    }

    public function assertTrue($condition, string $message = ''): void
    {
        $this->assertEquals(true, $condition, $message);
    }

    public function assertFalse($condition, string $message = ''): void
    {
        $this->assertEquals(false, $condition, $message);
    }
}

/** @return array<string, string> region world key => name of its getBoss('') boss, for regions that have one */
function region_bosses(World $world): array
{
    $bosses = [];
    foreach ($world->getRegions() as $key => $region) {
        if ($region->getBoss('') !== null) {
            $bosses[$key] = $region->getBoss('')->getName();
        }
    }
    return $bosses;
}

function region_keys(World $world): array
{
    $keys = [];
    foreach ($world->getRegions() as $key => $region) {
        $keys[spl_object_id($region)] = $key;
    }
    return $keys;
}

function dump_world(): array
{
    reset_vt();
    $world = new_world('standard');
    $lines = [];
    foreach (Item::all($world) as $item) {
        $power = $item instanceof Item\Upgrade\Health ? $item->power * 4 : 0;
        if ($power != (int) $power) {
            fail("heart power not in quarters: {$item->getRawName()}");
        }
        $target = $item instanceof ALttP\ItemAlias ? $item->getTarget()->getRawName() : '-';
        $lines[] = implode("\t", ['item', check_name($item->getRawName()), short_class($item), (int) $power, $target]);
    }
    [$counts, $values] = views(all_items_base($world));
    $lines[] = "all_items\tcounts\t" . encode_view($counts, []);
    $lines[] = "all_items\tvalues\t" . encode_view($values, []);
    foreach (Boss::all($world) as $boss) {
        $lines[] = "boss\t" . check_name($boss->getName());
    }
    foreach (VARIANTS as $variant) {
        reset_vt();
        $world = new_world($variant);
        $keys = region_keys($world);
        foreach ($world->getRegions() as $key => $region) {
            $bosses = '-';
            if ($region->getBoss('') !== null) {
                $bosses = 'boss=' . $region->getBoss('')->getName();
                if (property_exists($region, 'boss_top')) {
                    foreach (['top', 'middle', 'bottom'] as $level) {
                        $bosses .= "|$level=" . $region->getBoss($level)->getName();
                    }
                }
            }
            $region_items = array_map(fn ($i) => $i->getRawName(), prop($region, Region::class, 'region_items'));
            $prize = $region->getPrizeLocation();
            $lines[] = implode("\t", [
                'region', $variant, check_name($key), check_name($region->getName()), short_class($region),
                $prize ? (string) $prize : '-', $bosses, $region_items ? implode('|', $region_items) : '-',
            ]);
        }
        foreach ($world->getLocations() as $location) {
            $item = $location->getItem();
            $lines[] = implode("\t", [
                'location', $variant, check_name((string) $location), $keys[spl_object_id($location->getRegion())],
                short_class($location), $item ? $item->getRawName() : '-',
            ]);
        }
    }
    return $lines;
}

const PSEUDO_ITEMS = ['AnySword', 'AnyBow', 'AnyBottle', 'Gloves', 'Flute', 'UpgradedSword'];

/** @return array<string, ItemCollection> */
function multiset_family(World $world): array
{
    $add = function (ItemCollection $items, string $name, int $n = 1) use ($world): ItemCollection {
        for ($i = 0; $i < $n; $i++) {
            $items->addItem(Item::get($name, $world));
        }
        return $items;
    };
    $fresh = function () use ($world): ItemCollection {
        $items = new ItemCollection;
        $items->setChecksForWorld($world->id);
        return $items;
    };
    $names = array_keys(item_order());
    $family = ['empty' => $fresh()];
    foreach ($names as $name) {
        $family["one:$name"] = $add($fresh(), $name);
    }
    foreach ([2, 3, 4] as $n) {
        foreach ($names as $name) {
            $family["x$n:$name"] = $add($fresh(), $name, $n);
        }
    }
    $family['all'] = all_items_except($world, ['BigKey', 'Key', 'ShopKey']);
    foreach (array_merge($names, PSEUDO_ITEMS) as $name) {
        $family["all_except:$name"] = all_items_except($world, [$name]);
    }
    foreach ([1, 2, 3] as $n) {
        $family["uncle+$n"] = $add($add($fresh(), 'UncleSword'), 'ProgressiveSword', $n);
    }
    foreach (['HalfMagic', 'QuarterMagic'] as $magic) {
        foreach ([0, 1, 2, 3, 4] as $n) {
            $family["$magic+$n"] = $add($add($fresh(), $magic), 'Bottle', $n);
        }
    }
    return $family;
}

function predicate_columns(World $world): array
{
    $columns = [
        'canLiftRocks' => fn ($i) => $i->canLiftRocks(),
        'canLiftDarkRocks' => fn ($i) => $i->canLiftDarkRocks(),
        'canLightTorches' => fn ($i) => $i->canLightTorches(),
        'canMeltThings(w)' => fn ($i) => $i->canMeltThings($world),
        'canFly(w)' => fn ($i) => $i->canFly($world),
        'canSpinSpeed' => fn ($i) => $i->canSpinSpeed(),
        'canAcquireFairy' => fn ($i) => $i->canAcquireFairy(),
        'canAcquireFairy(w)' => fn ($i) => $i->canAcquireFairy($world),
        'canBunnyRevive' => fn ($i) => $i->canBunnyRevive(),
        'canBunnyRevive(w)' => fn ($i) => $i->canBunnyRevive($world),
        'canShootArrows(w,1)' => fn ($i) => $i->canShootArrows($world),
        'canShootArrows(w,2)' => fn ($i) => $i->canShootArrows($world, 2),
        'canBlockLasers' => fn ($i) => $i->canBlockLasers(),
        'canExtendMagic' => fn ($i) => $i->canExtendMagic(),
        'canExtendMagic(null,3)' => fn ($i) => $i->canExtendMagic(null, 3),
        'canExtendMagic(w,1)' => fn ($i) => $i->canExtendMagic($world, 1),
        'canExtendMagic(w,2)' => fn ($i) => $i->canExtendMagic($world, 2),
        'canExtendMagic(w,3)' => fn ($i) => $i->canExtendMagic($world, 3),
        'canExtendMagic(w,4)' => fn ($i) => $i->canExtendMagic($world, 4),
        'glitchedLinkInDarkWorld' => fn ($i) => $i->glitchedLinkInDarkWorld(),
        'hasHealth(7)' => fn ($i) => $i->hasHealth(7),
        'hasHealth(12)' => fn ($i) => $i->hasHealth(12),
        'heartCount*4' => fn ($i) => $i->heartCount() * 4,
        'canKillEscapeThings(w)' => fn ($i) => $i->canKillEscapeThings($world),
        'canKillMostThings(w)' => fn ($i) => $i->canKillMostThings($world),
        'canKillMostThings(w,8)' => fn ($i) => $i->canKillMostThings($world, 8),
        'canBombThings' => fn ($i) => $i->canBombThings(),
        'canGetGoodBee' => fn ($i) => $i->canGetGoodBee(),
        'hasSword(1)' => fn ($i) => $i->hasSword(),
        'hasSword(2)' => fn ($i) => $i->hasSword(2),
        'hasSword(3)' => fn ($i) => $i->hasSword(3),
        'hasSword(4)' => fn ($i) => $i->hasSword(4),
        'hasArmor(1)' => fn ($i) => $i->hasArmor(),
        'hasArmor(2)' => fn ($i) => $i->hasArmor(2),
        'hasBottle(1)' => fn ($i) => $i->hasBottle(),
        'hasBottle(2)' => fn ($i) => $i->hasBottle(2),
        'bottleCount' => fn ($i) => $i->bottleCount(),
        'hasABottle' => fn ($i) => $i->hasABottle(),
        'count' => fn ($i) => $i->count(),
        'has(KeyH2)' => fn ($i) => $i->has('KeyH2'),
        'has(KeyP1,2)' => fn ($i) => $i->has('KeyP1', 2),
        'has(BigKeyP1)' => fn ($i) => $i->has('BigKeyP1'),
        'has(ProgressiveSword,2)' => fn ($i) => $i->has('ProgressiveSword', 2),
        'has(ProgressiveSword,0)' => fn ($i) => $i->has('ProgressiveSword', 0),
    ];
    foreach (Boss::all($world) as $boss) {
        $columns['canBeat:' . $boss->getName()] = fn ($i) => $boss->canBeat($i);
    }
    return $columns;
}

function predicate_value($v): string
{
    if (is_bool($v)) {
        return bit($v);
    }
    if ($v != (int) $v) {
        fail("non-integer predicate value $v");
    }
    return (string) (int) $v;
}

function dump_predicates(): array
{
    reset_vt();
    $lines = [];
    $worlds = [];
    foreach (vector_configs() as $name => [$variant, $config]) {
        $worlds[$name] = new_world($variant, $config);
        $lines[] = "vector_config\t$name\t$variant";
        foreach (CONFIG_KEYS as $key) {
            $lines[] = "config\t$name\t$key\t" . json_encode($worlds[$name]->config($key));
        }
    }
    $columns = null;
    $families = [];
    foreach ($worlds as $name => $world) {
        $cols = predicate_columns($world);
        if ($columns === null) {
            $columns = array_keys($cols);
            $lines[] = "columns\t" . implode('|', array_map('check_name', $columns));
        }
        $families[$name] = [$world, multiset_family($world), $cols];
    }
    $first = array_key_first($families);
    $i = 0;
    foreach ($families[$first][1] as $label => $items) {
        [$counts, $values] = encode_items($items, $families[$first][0]);
        $lines[] = implode("\t", ['multiset', $i, $label, $counts, $values]);
        foreach ($families as $name => [$world, $family, $cols]) {
            if (encode_items($family[$label], $world) !== [$counts, $values]) {
                fail("multiset $label differs between configs");
            }
            $row = array_map(fn ($f) => predicate_value($f($family[$label])), $cols);
            $lines[] = implode("\t", ['pred', $i, $name, implode(' ', $row)]);
        }
        $i++;
    }
    return $lines;
}

// Each scenario: variant, placements, pre-collected items, then steps. A step is
// ['collect', start items] (collectItems with those starting items, or with none when null),
// ['reset'] (resetCollectedLocations), ['place', location, item], or ['fill', location, item] (Location::fill with
// an empty collection).
function collect_scenarios(): array
{
    return [
        'open_chain' => [
            'variant' => 'open',
            'placements' => [
                "Link's Uncle" => 'PegasusBoots',
                'Library' => 'PowerGlove',
                "Link's House" => 'Lamp',
                'King Zora' => 'Flippers',
                'Sewers - Dark Cross' => 'BookOfMudora',
                "Zora's Ledge" => 'Hammer',
                'Desert Ledge' => 'MoonPearl',
                'Ether Tablet' => 'ProgressiveSword',
                'Turtle Rock Medallion' => 'Quake',
            ],
            'precollected' => ['BugCatchingNet'],
            'steps' => [
                ['collect', null],
                ['collect', null],
                ['place', "Sahasrahla's Hut - Left", 'Bottle'],
                ['collect', null],
                ['place', "Link's House", 'Cape'],
                ['collect', ['Hookshot']],
                ['reset'],
                ['collect', ['Hookshot']],
            ],
        ],
        'standard_uncle' => [
            'variant' => 'standard',
            'placements' => ['Hyrule Castle - Map Chest' => 'KeyH2', 'Sanctuary' => 'PowerGlove'],
            'precollected' => [],
            'steps' => [
                ['fill', "Link's Uncle", 'UncleSword'],
                ['collect', null],
                ['reset'],
                ['collect', null],
            ],
        ],
        'standard_uncle_sticky' => [
            'variant' => 'standard',
            'placements' => ['Hyrule Castle - Map Chest' => 'KeyH2'],
            'precollected' => [],
            'steps' => [
                ['fill', "Link's Uncle", 'ProgressiveSword'],
                ['fill', "Link's Uncle", 'UncleSword'],
                ['reset'],
                ['fill', "Link's Uncle", 'KeyH2'],
                ['reset'],
                ['fill', "Link's Uncle", 'UncleSword'],
            ],
        ],
        'open_uncle' => [
            'variant' => 'open',
            'placements' => ['Secret Passage' => 'Lamp'],
            'precollected' => ['Flippers'],
            'steps' => [
                ['fill', "Link's Uncle", 'MapH2'],
                ['fill', "Link's Uncle", 'ProgressiveSword'],
                ['collect', ['PegasusBoots']],
            ],
        ],
        'inverted_merchant' => [
            'variant' => 'inverted',
            'placements' => ["Link's House" => 'MoonPearl'],
            'precollected' => [],
            'steps' => [
                ['collect', null],
                ['reset'],
                ['collect', ['DefeatAgahnim', 'Crystal5', 'Crystal6']],
                ['reset'],
                ['collect', ['DefeatAgahnim', 'Crystal5']],
            ],
        ],
    ];
}

function collected_set(World $world): string
{
    $collected = prop($world, World::class, 'collected_locations');
    $names = [];
    foreach ([$world->getLocations(), $world->getShops()->getLocations()] as $locations) {
        foreach ($locations as $location) {
            if ($collected[$location->getName()] ?? false) {
                $names[] = check_name((string) $location);
            }
        }
    }
    if (count($names) !== count(array_filter($collected))) {
        fail('collected location outside the world and its shops');
    }
    return $names ? implode('|', $names) : '-';
}

function new_collection(World $world, array $names): ItemCollection
{
    $items = new ItemCollection;
    foreach ($names as $name) {
        $items->addItem(Item::get($name, $world));
    }
    $items->setChecksForWorld($world->id);
    return $items;
}

function dump_collect(): array
{
    $lines = [];
    foreach (collect_scenarios() as $id => $s) {
        reset_vt();
        $world = new_world($s['variant']);
        $placements = [];
        foreach ($s['placements'] as $loc => $item) {
            $world->getLocation($loc)->setItem(Item::get($item, $world));
            $placements[] = "$loc=$item";
        }
        $pre = new_collection($world, $s['precollected']);
        $world->setPreCollectedItems($pre);
        [$pre_counts, $pre_values] = encode_items($pre, $world);
        $lines[] = implode("\t", [
            'collect_scenario', $id, $s['variant'], $placements ? implode('|', $placements) : '-', $pre_counts,
            $pre_values,
        ]);
        foreach ($s['steps'] as $n => $step) {
            $fields = ['collect_step', $id, $n, $step[0]];
            switch ($step[0]) {
                case 'collect':
                    $start = $step[1] === null ? null : new_collection($world, $step[1]);
                    $start_enc = $start === null ? ['-', '-'] : encode_items($start, $world);
                    $out = $world->collectItems($start);
                    array_push($fields, $start_enc[0], $start_enc[1], '-', ...encode_items($out, $world));
                    break;
                case 'reset':
                    $world->resetCollectedLocations();
                    array_push($fields, '-', '-', '-', '-', '-');
                    break;
                case 'place':
                    $world->getLocation($step[1])->setItem(Item::get($step[2], $world));
                    array_push($fields, $step[1], $step[2], '-', '-', '-');
                    break;
                case 'fill':
                    $ok = $world->getLocation($step[1])->fill(Item::get($step[2], $world), new ItemCollection);
                    array_push($fields, $step[1], $step[2], bit($ok), '-', '-');
                    break;
                default:
                    fail("unknown collect step {$step[0]}");
            }
            $fields[] = collected_set($world);
            $lines[] = implode("\t", $fields);
        }
    }
    return $lines;
}

// method => [query, collection source, location parameter, item parameter]; region-scoped queries read their
// region from the method source.
const METHOD_KINDS = [
    'testLocation' => ['access', 'collected', 'location', null],
    'testLocationWithMedallion' => ['access', 'collected', 'location', null],
    'testFillLocation' => ['fill', 'collected', 'location', 'item'],
    'testRegionLockedItems' => ['region_can_fill', null, null, 'item_name'],
    'testEntry' => ['enter', 'collected', null, null],
    'testNothingRequiredToEnter' => ['enter', 'collected', null, null],
    'testCanEnterWithEverything' => ['enter', 'built', null, null],
    'testCapeOrUpgradedSwordRequiredToEnter' => ['enter', 'built', null, null],
    'testSwordRequiredToComplete' => ['complete', 'built', null, null],
    'testComplete' => ['complete', 'collected', null, null],
    'testSetVanillaDungeonItems' => ['vanilla_dungeon_items', 'collected', null, null],
    'testVanillaSwordsSet' => ['prepared_item', 'collected', null, null],
    'testVanillaSwordsSetWithPedestalGoal' => ['prepared_item', 'collected', null, null],
];
const SKIPPED_METHODS = ['testDungeonItemWrite'];
// Test file under tests/ => the only methods run => region, for a region-scoped query whose region the source
// does not name with getRegion.
const TOP_LEVEL_METHODS = [
    'RegionTest.php' => ['testRegionLockedItems' => 'Region'],
    'WorldTest.php' => ['testSetVanillaDungeonItems' => null],
    'RetroBowTest.php' => ['testLocation' => null],
    'RandomizerTest.php' => ['testVanillaSwordsSet' => null, 'testVanillaSwordsSetWithPedestalGoal' => null],
];
const PREPARED_SEED = 1;
const PREPARED_KEYS = ['accessibility', 'mode.weapons', 'goal'];
const VANILLA_KEYS = ['rom.vanillaKeys', 'rom.vanillaBigKeys', 'rom.vanillaCompasses', 'rom.vanillaMaps'];
const WILD_KEYS = ['region.wildKeys', 'region.wildBigKeys', 'region.wildCompasses', 'region.wildMaps'];

function method_source(ReflectionMethod $m): string
{
    $lines = file($m->getFileName());
    return implode('', array_slice($lines, $m->getStartLine() - 1, $m->getEndLine() - $m->getStartLine() + 1));
}

function method_region(ReflectionMethod $m): string
{
    $source = method_source($m);
    preg_match_all("/getRegion\\('([^']+)'\\)/", $source, $matches);
    $regions = array_unique($matches[1]);
    if (count($regions) !== 1) {
        fail("{$m->class}::{$m->name}: expected one getRegion('...'), found " . count($regions));
    }
    return $regions[0];
}

function config_value_string($v): string
{
    if (!is_bool($v) && !is_int($v) && !(is_string($v) && !preg_match('/[\t|=,\n]/', $v))) {
        fail('unencodable config override: ' . var_export($v, true));
    }
    return json_encode($v);
}

function reference_world(string $variant): World
{
    static $worlds = [];
    return $worlds[$variant] ??= new_world($variant);
}

function scenario_from_capture(array $c, string $query, string $arg1, string $arg2): array
{
    $ref = reference_world($c['variant']);
    $overrides = [];
    foreach ($c['config'] as $key => $value) {
        if ($ref->config($key) !== $value) {
            $overrides[] = check_name($key) . '=' . config_value_string($value);
        }
    }
    $preset = [];
    foreach ($ref->getLocations() as $location) {
        $name = (string) $location;
        $item = $location->getItem();
        if (!array_key_exists($name, $c['locations'])) {
            fail("location $name missing from the test world");
        }
        $now = $c['locations'][$name];
        $was = $item ? $item->getRawName() : null;
        if ($now === $was) {
            continue;
        }
        if ($query === 'fill' && $name === $arg1 && $c['actual'] === true && $now === $arg2) {
            continue;
        }
        if ($now === null) {
            fail("location $name emptied by a test");
        }
        $preset[] = "$name=$now";
    }
    foreach (region_bosses($ref) as $key => $boss) {
        if (!array_key_exists($key, $c['bosses'])) {
            fail("region $key lost its boss in a test");
        }
        if ($c['bosses'][$key] !== $boss) {
            $preset[] = 'boss:' . check_name($key) . '=' . check_name($c['bosses'][$key]);
        }
    }
    return [
        'variant' => $c['variant'],
        'config' => $overrides ? implode(',', $overrides) : '-',
        'preset' => $preset ? implode('|', $preset) : '-',
        'counts' => $c['items'][0],
        'values' => $c['items'][1],
        'query' => $query,
        'arg1' => $arg1,
        'arg2' => $arg2,
    ];
}

// WorldTest::testSetVanillaDungeonItems's World::factory config, less what TEST_CONFIG already holds.
function vanilla_scenario(array $named): array
{
    $config = ['logic' => $named['logic']];
    foreach (WILD_KEYS as $key) {
        $config[$key] = $named['free'];
    }
    $overrides = [];
    foreach ($config as $key => $value) {
        if ((TEST_CONFIG[$key] ?? null) !== $value) {
            $overrides[] = check_name($key) . '=' . config_value_string($value);
        }
    }
    return [
        'variant' => 'standard',
        'config' => $overrides ? implode(',', $overrides) : '-',
        'preset' => '-',
        'counts' => 'E',
        'values' => 'E',
        'query' => 'vanilla_dungeon_items',
        'arg1' => '-',
        'arg2' => '-',
    ];
}

/** @return list<string> the getLocation("...") names of the method source, in order */
function method_locations(ReflectionMethod $m): array
{
    preg_match_all('/getLocation\("([^"]+)"\)/', method_source($m), $matches);
    return $matches[1];
}

function prepared_scenario(array $c, string $location, string $item): array
{
    if ($c['variant'] !== 'standard') {
        fail("prepared_item on a {$c['variant']} world");
    }
    $ref = reference_world('standard');
    $overrides = [];
    foreach ($c['config'] as $key => $value) {
        if ($ref->config($key) === $value) {
            continue;
        }
        if (!in_array($key, PREPARED_KEYS, true)) {
            fail("prepared_item override the C replay cannot map: $key");
        }
        $overrides[$key] = check_name($key) . '=' . config_value_string($value);
    }
    ksort($overrides, SORT_STRING);
    return [
        'variant' => 'standard',
        'config' => $overrides ? implode(',', $overrides) : '-',
        'preset' => '-',
        'counts' => 'E',
        'values' => 'E',
        'query' => 'prepared_item',
        'arg1' => check_name($location),
        'arg2' => check_name($item),
    ];
}

function prepared_seed(): void
{
    prng_seed(PREPARED_SEED);
    prng_trace_start(0);
}

function parse_config_value(string $s)
{
    return json_decode($s, false, 512, JSON_THROW_ON_ERROR);
}

function parse_overrides(string $config): array
{
    $overrides = [];
    if ($config !== '-') {
        foreach (explode(',', $config) as $entry) {
            [$key, $value] = explode('=', $entry, 2);
            $overrides[$key] = parse_config_value($value);
        }
    }
    return $overrides;
}

function replay(array $s): bool
{
    reset_vt();
    if ($s['query'] === 'vanilla_dungeon_items') {
        $world = World::factory($s['variant'], array_merge(TEST_CONFIG, parse_overrides($s['config'])));
        $flags = array_map(fn ($key) => $world->config($key), VANILLA_KEYS);
        if (count(array_unique($flags, SORT_REGULAR)) !== 1 || !is_bool($flags[0])) {
            fail('the four rom.vanilla* keys differ or are not booleans: ' . json_encode($flags));
        }
        return $flags[0];
    }
    if ($s['query'] === 'prepared_item') {
        $world = World::factory($s['variant'], array_merge(TEST_CONFIG, parse_overrides($s['config'])));
        prepared_seed();
        (new Randomizer([$world]))->prepareWorld($world);
        $item = $world->getLocation($s['arg1'])->getItem();
        return $item !== null && $item->getRawName() === $s['arg2'];
    }
    $overrides = parse_overrides($s['config']);
    $factory = array_key_exists('logic', $overrides) ? ['logic' => $overrides['logic']] : [];
    $world = new_world($s['variant'], array_merge(TEST_CONFIG, $factory));
    foreach ($overrides as $key => $value) {
        $world->testSetConfig($key, $value);
    }
    if ($s['preset'] !== '-') {
        foreach (explode('|', $s['preset']) as $entry) {
            [$target, $value] = explode('=', $entry);
            if (str_starts_with($target, 'boss:')) {
                $all = Boss::all($world);
                $world->getRegion(substr($target, 5))->setBoss(isset($all[$value]) ? $all[$value] : new Boss($value));
            } else {
                $world->getLocation($target)->setItem(Item::get($value, $world));
            }
        }
    }
    $items = decode_items($s['counts'], $s['values'], $world);
    switch ($s['query']) {
        case 'access':
            return $world->getLocation($s['arg1'])->canAccess($items);
        case 'fill':
            return $world->getLocation($s['arg1'])->fill(Item::get($s['arg2'], $world), $items);
        case 'region_can_fill':
            $region = $s['arg1'] === 'Region' ? new Region($world) : $world->getRegion($s['arg1']);
            return $region->canFill(Item::get($s['arg2'], $world));
        case 'enter':
            return $world->getRegion($s['arg1'])->canEnter($world->getLocations(), $items);
        case 'complete':
            return $world->getRegion($s['arg1'])->canComplete($world->getLocations(), $items);
    }
    fail("unknown query {$s['query']}");
}

function load_test_class(string $dir, string $file): string
{
    $before = get_declared_classes();
    require_once "$dir/$file";
    $new = array_values(array_filter(array_diff(get_declared_classes(), $before),
        fn ($c) => is_subclass_of($c, TestCase::class)));
    if (count($new) !== 1) {
        fail("$file declares " . count($new) . ' test classes');
    }
    return $new[0];
}

/** @return list<array{0: string, 1: string, 2: ?array<string, ?string>}> file, class, methods run (null: all) */
function test_sources(string $vt_dir, string $tree): array
{
    $dir = "$vt_dir/tests/$tree";
    $files = [];
    $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS));
    foreach ($it as $file) {
        if (str_ends_with($file->getFilename(), 'Test.php')) {
            $files[] = substr($file->getPathname(), strlen($dir) + 1);
        }
    }
    sort($files, SORT_STRING);
    $sources = [];
    foreach ($files as $file) {
        if ($tree === 'NoGlitches' && array_key_exists($file, TOP_LEVEL_METHODS)) {
            fail("tests/NoGlitches/$file has the name of a top-level test file");
        }
        $sources[] = [$file, load_test_class($dir, $file), null];
    }
    if ($tree === 'NoGlitches') {
        foreach (TOP_LEVEL_METHODS as $file => $methods) {
            $sources[] = [$file, load_test_class("$vt_dir/tests", $file), $methods];
        }
    }
    return $sources;
}

function row_line(array $where, array $s, bool $expected, bool $answer): string
{
    return implode("\t", array_merge(['row'], $where, [
        $s['variant'], $s['config'], $s['preset'], $s['counts'], $s['values'], $s['query'], $s['arg1'], $s['arg2'],
        bit($expected), bit($answer),
    ]));
}

function prepared_rows(string $file, ReflectionMethod $m, int $index, array $c, array &$mismatches): array
{
    $what = "{$m->class}::{$m->name} row $index";
    $locations = method_locations($m);
    if (!is_array($c['expected']) || !is_array($c['actual']) || count($c['expected']) !== count($locations)
        || count($c['actual']) !== count($locations)) {
        fail("$what: " . count($locations) . ' locations for the assertion\'s item lists');
    }
    $lines = [];
    foreach ($locations as $i => $location) {
        $want = $c['expected'][$i]->getRawName();
        $have = $c['actual'][$i];
        $answer = $have !== null && $have->getRawName() === $want;
        $s = prepared_scenario($c, $location, $want);
        if (replay($s) !== $answer) {
            fail("$what location $i: the recorded scenario does not replay VT's answer");
        }
        $line = row_line([$file, $m->name, '-', $i], $s, true, $answer);
        if (!$answer) {
            $mismatches[] = $line;
        }
        $lines[] = $line;
    }
    return $lines;
}

function dump_rows(string $vt_dir, string $tree, array &$mismatches): array
{
    $lines = [];
    foreach (test_sources($vt_dir, $tree) as [$file, $class, $only]) {
        $refl = new ReflectionClass($class);
        foreach ($refl->getMethods(ReflectionMethod::IS_PUBLIC) as $m) {
            if ($m->class !== $class || !str_starts_with($m->name, 'test')) {
                continue;
            }
            if (in_array($m->name, SKIPPED_METHODS, true) || ($only !== null && !array_key_exists($m->name, $only))) {
                continue;
            }
            $kind = METHOD_KINDS[$m->name] ?? fail("$class::{$m->name}: unknown test method");
            [$query, $source, $location_param, $item_param] = $kind;
            $region = in_array($query, ['region_can_fill', 'enter', 'complete'], true)
                ? ($only[$m->name] ?? method_region($m)) : null;
            $provider = preg_match('/@dataProvider\s+(\w+)/', (string) $m->getDocComment(), $pm) ? $pm[1] : null;
            if ($provider === null && $m->getNumberOfParameters() > 0) {
                fail("$class::{$m->name}: parameters without a data provider");
            }
            $rows = $provider === null ? [[]] : (new $class)->$provider();
            $params = array_map(fn ($p) => $p->getName(), $m->getParameters());
            foreach (array_values($rows) as $index => $row) {
                reset_vt();
                $test = new $class;
                $test->collection_source = $source ?? 'collected';
                $test->setUp();
                if ($query === 'prepared_item') {
                    prepared_seed();
                }
                $test->{$m->name}(...$row);
                $assertions = $query === 'vanilla_dungeon_items' ? count(VANILLA_KEYS) : 1;
                if (count($test->captures) !== $assertions) {
                    fail("$class::{$m->name} row $index: " . count($test->captures) . ' assertions');
                }
                $c = $test->captures[0];
                foreach ($test->captures as $other) {
                    if ($other['expected'] !== $c['expected'] || $other['actual'] !== $c['actual']) {
                        fail("$class::{$m->name} row $index: assertions with different values");
                    }
                }
                $named = [];
                foreach ($params as $i => $p) {
                    $named[$p] = array_key_exists($i, $row) ? $row[$i] : null;
                }
                if ($query === 'prepared_item') {
                    array_push($lines, ...prepared_rows($file, $m, $index, $c, $mismatches));
                    continue;
                }
                if (!is_bool($c['actual']) || !is_bool($c['expected'])) {
                    fail("$class::{$m->name} row $index: non-boolean assertion");
                }
                if ($query === 'vanilla_dungeon_items') {
                    $s = vanilla_scenario($named);
                } else {
                    $arg1 = $region ?? $named[$location_param];
                    $arg2 = $item_param === null ? '-'
                        : Item::get($named[$item_param], $test->worldUnderTest())->getRawName();
                    $s = scenario_from_capture($c, $query, check_name($arg1), check_name($arg2));
                }
                if (replay($s) !== $c['actual']) {
                    fail("$class::{$m->name} row $index: the recorded scenario does not replay VT's answer");
                }
                $line = row_line([$file, $m->name, $provider ?? '-', $index], $s, $c['expected'], $c['actual']);
                if ($c['expected'] !== $c['actual']) {
                    $mismatches[] = $line;
                }
                $lines[] = $line;
            }
        }
    }
    return $lines;
}

$world_lines = dump_world();
$predicate_lines = dump_predicates();
$row_lines = [];
$mismatches = [];
foreach (array_keys(ROW_TREES) as $tree) {
    $mismatches[$tree] = [];
    $row_lines[$tree] = dump_rows($vt_dir, $tree, $mismatches[$tree]);
}
$collect_lines = dump_collect();

$headers = [
    'world_dump.txt' => [
        "# VT's world structure at the pinned commit, written by extract_logic.php (run by gen_golden.sh).",
        '# Checked against the C tables by test_items and test_world --fixtures. Fields are tab-separated.',
        '# item <raw name> <VT class> <heart power in quarter hearts, 0 if not Health> <alias target or ->',
        '#   in Item::all order',
        '# all_items <counts|values> <view>: Item::all(world)->copy()->manyKeys(), the base of allItemsExcept',
        '# boss <name>: in Boss::all order',
        '# region <variant> <world key> <spoiler name> <VT class> <prize location or -> <boss slots or -> <region items or ->',
        '#   in World order; boss slots are boss=<name>, plus |top=|middle=|bottom= for Ganons Tower',
        '# location <variant> <name> <region world key> <VT class> <item set by the constructor or ->',
        '#   in World::getLocations() order',
        '# A view is E (from empty) or A (from the all_items view), then :<name>=<count>|... for the counts',
        '#   that differ from that base (0 removes); lists are |-separated.',
    ],
    'predicate_vectors.txt' => [
        "# VT's ItemCollection predicates and Boss::canBeat, written by extract_logic.php (run by gen_golden.sh).",
        '# Checked against the C port by test_items and test_bosses --fixtures. Fields are tab-separated.',
        '# vector_config <config> <variant>, then config <config> <key> <JSON of World::config(key), no default>',
        '#   test: the NoGlitches tests\' Standard world; baseline: the baseline case; basic_swordless: baseline',
        '#   with itemPlacement basic, mode.weapons swordless and item.functionality expert',
        '#   inverted: the test config on an Inverted world',
        '#   retro: the test config on a Retro world',
        '# columns <predicate>|...: the order of the values of every pred line; (w) passes the config\'s world',
        '# multiset <index> <label> <counts view> <values view>: built with VT operations (views as in world_dump.txt)',
        '# pred <index> <config> <values, space-separated>: 1/0 for booleans, integers otherwise (heartCount in quarter hearts)',
    ],
    'noglitches_rows.txt' => [
        "# VT's tests/NoGlitches rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_world and test_regions --fixtures check the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/NoGlitches> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
        '#   RegionTest.php and WorldTest.php are under tests/, not tests/NoGlitches; region Region is a bare Region',
        '#   RetroBowTest.php is under tests/ too; its rows are on a Retro world (variant retro)',
        '#   vanilla_dungeon_items: World::factory with the config overrides added to the test config; the answer is',
        '#   the value all four rom.vanilla* keys share',
        '#   RandomizerTest.php is under tests/ too; only testVanillaSwordsSet and testVanillaSwordsSetWithPedestalGoal run,',
        '#   one row per asserted location, the index being its position in the assertion',
        '#   prepared_item: World::factory with the config overrides added to the test config, prng_seed(' . PREPARED_SEED . '),',
        '#   Randomizer::prepareWorld; the answer is whether the location holds the item (raw name)',
        '#   accessibility "full" is VT\'s legacy value, which VT reads as "items": a replay uses items',
        '# collect_scenario <id> <variant> <placements or -> <pre-collected counts> <pre-collected values>',
        '# collect_step <id> <n> <op> <a> <b> <answer or -> <counts or -> <values or -> <collected locations or ->',
        '#   collect: collectItems(start), a/b the start views or - for none, counts/values its result',
        '#   reset: resetCollectedLocations(); place: Location::setItem(b) on location a;',
        '#   fill: Location::fill(b) on location a with an empty collection; collected: the memo after the step,',
        '#   World::getLocations() order then the shops\' locations (VT merges shop items into collectItems)',
    ],
    'overworld_glitches_rows.txt' => [
        "# VT's tests/OverworldGlitches rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_regions --fixtures checks the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/OverworldGlitches> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   a logic override is also passed to World::factory with the test config, before the testSetConfig calls',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
    ],
    'major_glitches_rows.txt' => [
        "# VT's tests/MajorGlitches rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_regions --fixtures checks the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/MajorGlitches> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   a logic override is also passed to World::factory with the test config, before the testSetConfig calls',
        '#   rom.freeItemMenu is ROM-only (VT\'s constructor derives it from glitched logic); a replay may ignore it',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
    ],
    'inverted_rows.txt' => [
        "# VT's tests/Inverted rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_regions --fixtures checks the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/Inverted> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#     or boss:<region world key>=<boss>, set with Region::setBoss (a name outside Boss::all: a Boss with no rule)',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
    ],
    'inverted_overworld_glitches_rows.txt' => [
        "# VT's tests/InvertedOverworldGlitches rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_regions --fixtures checks the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/InvertedOverworldGlitches> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   a logic override is also passed to World::factory with the test config, before the testSetConfig calls',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#     or boss:<region world key>=<boss>, set with Region::setBoss (a name outside Boss::all: a Boss with no rule)',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
    ],
    'inverted_major_glitches_rows.txt' => [
        "# VT's tests/InvertedMajorGlitches rows run inside VT, written by extract_logic.php (run by gen_golden.sh).",
        '# Each row was replayed by VT from its record alone; test_regions --fixtures checks the C port.',
        '# Fields are tab-separated; views as in world_dump.txt.',
        '# row <test file under tests/InvertedMajorGlitches> <method> <provider or -> <index> <variant> <config overrides or -> <preset items or ->',
        '#   <counts view> <values view> <query> <location or region> <item or -> <test expectation> <VT answer>',
        '#   config overrides: <key>=<JSON>,... applied with World::testSetConfig on the World::factory test world',
        '#   a logic override is also passed to World::factory with the test config, before the testSetConfig calls',
        '#   rom.freeItemMenu is ROM-only (VT\'s constructor derives it from glitched logic); a replay may ignore it',
        '#   preset items: <location>=<item>|... set with Location::setItem before the query',
        '#     or boss:<region world key>=<boss>, set with Region::setBoss (a name outside Boss::all: a Boss with no rule)',
        '#   queries: access (Location::canAccess), fill (Location::fill), region_can_fill (Region::canFill),',
        '#   enter (Region::canEnter), complete (Region::canComplete); the C port must return the VT answer',
    ],
];
$bodies = [
    'world_dump.txt' => $world_lines,
    'predicate_vectors.txt' => $predicate_lines,
];
foreach (ROW_TREES as $tree => $file) {
    $bodies[$file] = $tree === 'NoGlitches' ? array_merge($row_lines[$tree], $collect_lines) : $row_lines[$tree];
}
if (!is_dir($out_dir) && !mkdir($out_dir, 0777, true)) {
    fail("cannot create $out_dir");
}
foreach ($bodies as $file => $body) {
    file_put_contents("$out_dir/$file", implode("\n", array_merge($headers[$file], $body)) . "\n");
}
foreach ($mismatches as $tree => $lines) {
    foreach ($lines as $line) {
        fwrite(STDERR, "extract_logic: VT disagrees with its $tree test: $line\n");
    }
}
$summary = [];
foreach (array_keys(ROW_TREES) as $tree) {
    $summary[] = sprintf('%d %s rows (%d VT answers differ from the test)', count($row_lines[$tree]), $tree,
        count($mismatches[$tree]));
}
printf("extract_logic: %d world records, %d predicate records, %s, %d collect records\n", count($world_lines),
    count($predicate_lines), implode(', ', $summary), count($collect_lines));
