<?php
// The logging Rom, Text, Credits and InitialSram behind rom_writes.txt and rom_buffers.txt.
// Required by gen_case.php and text_default.php after oracle_boot_vt(): the classes extend VT's.

use ALttP\Rom;
use ALttP\Support\Credits;
use ALttP\Support\InitialSram;

// rom_buffers.txt's lines in call order; null while no Rom logs (a table being set up).
final class OracleBufferLog
{
    public static ?array $lines = null;

    public static function add(string $line): void
    {
        if (self::$lines !== null) {
            self::$lines[] = $line;
        }
    }
}

function oracle_hex(array $bytes): string
{
    return $bytes === [] ? '-' : strtoupper(bin2hex(pack('C*', ...$bytes)));
}

/**
 * The function called by the innermost World::writeToRom() frame, skipping Collection
 * methods and closures; writeToRom itself for a write from its body; else the write label.
 * frames[0] is the write. Throws on a write with neither an anchor nor a label.
 */
function oracle_write_tag(array $frames, ?string $label): string
{
    $tag = null;
    for ($i = count($frames) - 1; $i >= 1; --$i) {
        if ($frames[$i]['function'] !== 'writeToRom' || !is_a($frames[$i]['class'] ?? '', ALttP\World::class, true)) {
            continue;
        }
        $tag = 'writeToRom';
        for ($j = $i - 1; $j >= 1; --$j) {
            if (str_contains($frames[$j]['function'], '{closure}')
                || is_a($frames[$j]['class'] ?? '', ALttP\Support\Collection::class, true)) {
                continue;
            }
            $tag = $frames[$j]['function'];
            break;
        }
    }
    $tag ??= $label;
    if ($tag === null || strpos($tag, ' ') !== false) {
        throw new \Error('Rom::write(): write outside World::writeToRom() with no write label');
    }
    return $tag;
}

final class OracleText extends ALttP\Text
{
    /** @return array<string,array<int>> the table in its order */
    public function table(): array
    {
        return $this->text_array;
    }

    public function setString(string $id, string $value, ...$flags)
    {
        parent::setString($id, $value, ...$flags);
        $this->logId($id);
    }

    public function setStringRaw(string $id, array $rawvalue)
    {
        parent::setStringRaw($id, $rawvalue);
        $this->logId($id);
    }

    private function logId(string $id): void
    {
        OracleBufferLog::add("text $id " . oracle_hex($this->text_array[$id]));
    }
}

final class OracleCredits extends Credits
{
    public function updateCreditLine(string $scene, int $line, string $text, string $align = 'center')
    {
        $r = parent::updateCreditLine($scene, $line, $text, $align);
        if ($r !== false) {
            $stored = (new ReflectionProperty(Credits::class, 'scenes'))->getValue($this)[$scene][$line]['text'];
            OracleBufferLog::add("credit $scene $line $align " . oracle_hex(array_values(unpack('C*', $stored) ?: [])));
        }
        return $r;
    }
}

final class OracleInitialSram extends InitialSram
{
    private int $depth = 0;

    public function bytes(): array
    {
        return (new ReflectionProperty(InitialSram::class, 'initial_sram_bytes'))->getValue($this);
    }

    private function logged(string $method, callable $call)
    {
        $before = $this->depth === 0 ? $this->bytes() : null;
        ++$this->depth;
        try {
            $r = $call();
        } finally {
            --$this->depth;
        }
        if ($before !== null) {
            $after = $this->bytes();
            $keys = array_unique(array_merge(array_keys($before), array_keys($after)));
            sort($keys);
            $changes = [];
            foreach ($keys as $k) {
                if (($before[$k] ?? null) !== ($after[$k] ?? null)) {
                    $changes[] = sprintf('%03X=%02X', $k, $after[$k]);
                }
            }
            OracleBufferLog::add("sram $method " . ($changes === [] ? '-' : implode(',', $changes)));
        }
        return $r;
    }

    function setValue(int $idx, int $val)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setValue($idx, $val));
    }

    public function preOpenAgaCurtains()
    {
        return $this->logged(__FUNCTION__, fn () => parent::preOpenAgaCurtains());
    }

    public function preOpenSkullWoodsCurtains()
    {
        return $this->logged(__FUNCTION__, fn () => parent::preOpenSkullWoodsCurtains());
    }

    public function preOpenCastleGate()
    {
        return $this->logged(__FUNCTION__, fn () => parent::preOpenCastleGate());
    }

    public function preOpenGanonsTower()
    {
        return $this->logged(__FUNCTION__, fn () => parent::preOpenGanonsTower());
    }

    public function preOpenPyramid()
    {
        return $this->logged(__FUNCTION__, fn () => parent::preOpenPyramid());
    }

    public function setStartingEquipment(ALttP\Support\ItemCollection $items, $config)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setStartingEquipment($items, $config));
    }

    public function setProgressIndicator(int $indicator)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setProgressIndicator($indicator));
    }

    public function setProgressFlags(int $flags)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setProgressFlags($flags));
    }

    public function setStartingEntrance(int $entrance)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setStartingEntrance($entrance));
    }

    public function setStartingTimer(int $seconds)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setStartingTimer($seconds));
    }

    public function setSwordlessCurtains()
    {
        return $this->logged(__FUNCTION__, fn () => parent::setSwordlessCurtains());
    }

    public function setInstantPostAga(string $state)
    {
        return $this->logged(__FUNCTION__, fn () => parent::setInstantPostAga($state));
    }
}

final class OracleRom extends Rom
{
    /** @var list<array{int,string,string}> address, bytes, tag of each logged write */
    public array $writes = [];
    public ?string $label = null;

    /** Starts OracleBufferLog::$lines afresh. */
    public function __construct()
    {
        parent::__construct();
        (new ReflectionProperty(Rom::class, 'text'))->setValue($this, oracle_default_text());
        (new ReflectionProperty(Rom::class, 'credits'))->setValue($this, new OracleCredits);
        $this->initial_sram = new OracleInitialSram;
        OracleBufferLog::$lines = [];
    }

    public function write(int $offset, string $data, bool $log = true): Rom
    {
        if ($log) {
            if ($data === '') {
                throw new \Error(sprintf('Rom::write(): empty write at %06X', $offset));
            }
            $this->writes[] = [$offset, $data, oracle_write_tag(debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS), $this->label)];
        }
        return parent::write($offset, $data, $log);
    }
}

/** The text table as the Rom constructor leaves it, with logging off. */
function oracle_default_text(): OracleText
{
    $lines = OracleBufferLog::$lines;
    OracleBufferLog::$lines = null;
    $text = new OracleText;
    $text->removeUnwanted();
    OracleBufferLog::$lines = $lines;
    return $text;
}

/** @return list<string> "<id> <hex>" per text id, in table order; throws on an id with a space */
function oracle_text_lines(OracleText $text): array
{
    $lines = [];
    foreach ($text->table() as $id => $bytes) {
        if (strpos((string) $id, ' ') !== false || (string) $id === '') {
            throw new \Error("text id '$id' cannot be written on one line");
        }
        $lines[] = "$id " . oracle_hex($bytes);
    }
    return $lines;
}

/** Dies through $die unless every public InitialSram setter is logged by OracleInitialSram. */
function oracle_check_sram_overrides(callable $die): void
{
    foreach ((new ReflectionClass(OracleInitialSram::class))->getMethods(ReflectionMethod::IS_PUBLIC) as $m) {
        if (!$m->isStatic() && !in_array($m->getName(), ['__construct', 'getValue', 'getInitialSram', 'bytes'], true)
            && $m->getDeclaringClass()->getName() !== OracleInitialSram::class) {
            $die("InitialSram::{$m->getName()} is not logged");
        }
    }
}
