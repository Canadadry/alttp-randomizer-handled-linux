<?php
// Case loading and per-attempt world setup shared by gen_case.php and dump_pools.php.

const CONFIG_KEYS = [
    'glitches', 'item_placement', 'dungeon_items', 'accessibility', 'goal', 'crystals_tower',
    'crystals_ganon', 'state', 'weapons', 'item_pool', 'item_functionality',
];
const LOGIC_BY_GLITCHES = [
    'none' => 'NoGlitches',
    'overworld_glitches' => 'OverworldGlitches',
    'hybrid_major_glitches' => 'HybridMajorGlitches',
    'major_glitches' => 'MajorGlitches',
    'no_logic' => 'NoLogic',
];

function die_case(string $case, string $why): never
{
    fwrite(STDERR, basename($_SERVER['argv'][0], '.php') . ": $case: $why\n");
    exit(1);
}

/** @return array{name: string, config: array<string,string>, seed: int, logic: string} dies on an invalid case */
function oracle_load_case(string $case_dir): array
{
    $case = basename($case_dir);
    $config = json_decode((string) @file_get_contents("$case_dir/config.json"), true);
    if (!is_array($config)) {
        die_case($case, 'config.json is missing or not a JSON object');
    }
    foreach (CONFIG_KEYS as $key) {
        if (!isset($config[$key]) || !is_string($config[$key])) {
            die_case($case, "config.json: $key is missing or not a string");
        }
    }
    foreach (array_keys($config) as $key) {
        if (!in_array($key, CONFIG_KEYS, true)) {
            die_case($case, "config.json: unknown key $key");
        }
    }
    $seed_text = (string) @file_get_contents("$case_dir/seed");
    if (!preg_match('/^[0-9]{1,10}\n?$/', $seed_text) || (int) $seed_text > 0xFFFFFFFF) {
        die_case($case, 'seed is not a decimal uint32 on one line');
    }
    $logic = LOGIC_BY_GLITCHES[$config['glitches']]
        ?? die_case($case, "config.json: unknown glitches {$config['glitches']}");
    return ['name' => $case, 'config' => $config, 'seed' => (int) $seed_text, 'logic' => $logic];
}

function oracle_boot_vt(string $vt_dir): void
{
    ini_set('memory_limit', '512M');
    require "$vt_dir/vendor/autoload.php";
    $app = require "$vt_dir/bootstrap/app.php";
    $app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
    config([
        'logging.channels.oracle' => ['driver' => 'monolog', 'handler' => Monolog\Handler\NullHandler::class],
        'logging.default' => 'oracle',
    ]);

    // Seed::boot() builds a Hashids, which needs bcmath or gmp, only for a DB-save listener the oracle
    // never fires. Its throw still marks Seed booted, so World::factory() no longer throws.
    if (!extension_loaded('bcmath') && !extension_loaded('gmp')) {
        try {
            new ALttP\Seed;
        } catch (RuntimeException $e) {
        }
    }
}

/**
 * Starts the attempt's trace, reseeds, draws the random crystal counts and builds the world.
 */
function oracle_attempt_world(array $case, int $attempt): ALttP\World
{
    $config = $case['config'];
    ALttP\Item::clearCache();
    ALttP\Boss::clearCache();
    (new ReflectionProperty(ALttP\World::class, 'max_world'))->setValue(null, 1);
    prng_trace_start($attempt);
    prng_seed(prng_splitmix32(($case['seed'] + $attempt) & 0xFFFFFFFF));

    // prepSeed's order: ganon, then tower.
    prng_trace_label('crystals');
    $crystals_ganon = $config['crystals_ganon'] === 'random' ? get_random_int(0, 7) : $config['crystals_ganon'];
    $crystals_tower = $config['crystals_tower'] === 'random' ? get_random_int(0, 7) : $config['crystals_tower'];
    prng_trace_label(null);

    return ALttP\World::factory($config['state'], [
        'itemPlacement' => $config['item_pool'] === 'crowd_control' ? 'advanced' : $config['item_placement'],
        'dungeonItems' => $config['dungeon_items'],
        'accessibility' => $config['accessibility'],
        'goal' => $config['goal'],
        'crystals.ganon' => $crystals_ganon,
        'crystals.tower' => $crystals_tower,
        'entrances' => 'none',
        'mode.weapons' => $config['weapons'],
        'tournament' => false,
        'spoilers' => 'on',
        'allow_quickswap' => true,
        'override_start_screen' => false,
        'pseudoboots' => false,
        'spoil.Hints' => 'on',
        'logic' => $case['logic'],
        'item.pool' => $config['item_pool'],
        'item.functionality' => $config['item_functionality'],
        'enemizer.bossShuffle' => 'none',
        'enemizer.enemyShuffle' => 'none',
        'enemizer.enemyDamage' => 'default',
        'enemizer.enemyHealth' => 'default',
        'enemizer.potShuffle' => 'off',
    ]);
}
