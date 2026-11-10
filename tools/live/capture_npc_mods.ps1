# gl-specials-live-capture v2
# Read-only helper for the Phase 1b2b2d live NPC-mod oracle.
#
# It opens the original game process with PROCESS_VM_READ|PROCESS_QUERY_INFORMATION only,
# reads the NPC main container (+0x10), its item records and their mod records, and writes
# the capture file consumed by tests/game/test_special_gates.cpp (GL_SPECIALS_LIVE).
# It does not write memory, inject code, modify the process, or touch DRM.
#
# Live read (32-bit GoldenLand.exe / Server.dll):
#   entity +0x10              container id (resolve with FUN_1404bf8c or supply the container)
#   container +0x0c           pointer to u32 record-id array
#   container +0x10           slot count
#   container +0x14           used-slot bitmap
#   manager DAT_14088df8      RVA 0x88df8; manager+0x18 -> array of item-record pointers
#   item record +0x0c         item flags
#   item record +0xa4         record uid
#   item record +0xa8         item id
#   item record +0xb0         count
#   item record +0x78         mod count
#   item record +0x7c         pointer to mod array
#   mod record, stride 0x14   +0x00 id, +0x04 value (i32), +0x08 flags
#
# Capture format (gl-specials-live-2):
#   format=gl-specials-live-2
#   level=<level id, e.g. l1_1>
#   seed=<u32 seed metadata; optional for the oracle>
#   rng_state=<u32 MSVC CRT _rand holdrand immediately before the first NPC R1>
#   loot_level=<int loot level passed to generation>
#   item|person|item_id|count|item_flags|mod_count
#   mod|person|item_id|count|item_flags|mod_id|value|mod_flags
#
# A state captured at a breakpoint before the first NPC generation is authoritative;
# a seed alone is not sufficient when previous draws may have advanced the stream.
#
# Containers must be supplied as "Person=0x<container address>" (from a debugger); records
# can be supplied directly as "Person=0x<item record address>" for debugging. The Server.dll
# base is auto-detected when possible; otherwise pass the ASLR-resolved -ServerBase.

param(
    [Parameter(Mandatory = $true)][string]$OutFile,
    [Parameter(Mandatory = $true)][string]$Level,
    [Parameter(Mandatory = $true)][uint32]$RngState,
    [Parameter(Mandatory = $true)][int]$LootLevel,
    [uint32]$Seed = 0,
    [string]$ProcessName = "GoldenLand",
    [string[]]$NpcContainer = @(),
    [string[]]$Item = @(),
    [uint64]$Manager = 0,
    [uint64]$ServerBase = 0
)

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class GLSpecialsMem {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer,
                                                int size, out IntPtr read);
    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
"@

if ($LootLevel -lt 1) { throw "LootLevel must be positive" }
if ($NpcContainer.Count -eq 0 -and $Item.Count -eq 0) {
    throw "Supply at least one -NpcContainer or -Item target"
}

function Convert-Address([string]$text) {
    $value = $text.Trim()
    if ($value.StartsWith("0x") -or $value.StartsWith("0X")) {
        return [uint64][Convert]::ToUInt64($value.Substring(2), 16)
    }
    return [uint64]::Parse($value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Convert-Targets([string[]]$targets) {
    $parsed = @()
    foreach ($target in $targets) {
        $index = $target.IndexOf("=")
        if ($index -lt 1) { throw "Target must be Person=0xADDRESS: $target" }
        $parsed += [pscustomobject]@{
            Person  = $target.Substring(0, $index)
            Address = Convert-Address $target.Substring($index + 1)
        }
    }
    return $parsed
}

$process = Get-Process -Name $ProcessName -ErrorAction Stop | Select-Object -First 1
$handle = [GLSpecialsMem]::OpenProcess(0x0410, $false, $process.Id)
if ($handle -eq [IntPtr]::Zero) {
    throw "OpenProcess failed: $([ComponentModel.Win32Exception]::new([Runtime.InteropServices.Marshal]::GetLastWin32Error()).Message)"
}

function Read-Bytes([uint64]$address, [int]$size) {
    $buffer = New-Object byte[] $size
    $read = [IntPtr]::Zero
    if (-not [GLSpecialsMem]::ReadProcessMemory($handle, [IntPtr]::new([int64]$address), $buffer,
                                                $size, [ref]$read) -or $read.ToInt64() -ne $size) {
        throw ("ReadProcessMemory failed at 0x{0:x}" -f $address)
    }
    return $buffer
}

function Read-U32([uint64]$address) {
    return [BitConverter]::ToUInt32((Read-Bytes $address 4), 0)
}

function Read-I32([uint64]$address) {
    return [BitConverter]::ToInt32((Read-Bytes $address 4), 0)
}

if ($ServerBase -eq 0) {
    $module = $process.Modules | Where-Object { $_.ModuleName -ieq "Server.dll" } |
        Select-Object -First 1
    if ($null -ne $module) { $ServerBase = [uint64]$module.BaseAddress.ToInt64() }
}
$managerAddress = $Manager
if ($managerAddress -eq 0 -and $ServerBase -ne 0) {
    $managerAddress = Read-U32 ($ServerBase + 0x88df8)
}

function Add-ItemRecord([System.Text.StringBuilder]$out, [string]$person, [uint64]$record) {
    $uid = Read-U32 ($record + 0xa4)
    $itemId = Read-U32 ($record + 0xa8)
    $flags = Read-U32 ($record + 0x0c)
    $count = Read-I32 ($record + 0xb0)
    $modCount = Read-U32 ($record + 0x78)
    $mods = Read-U32 ($record + 0x7c)
    if ($uid -eq 0) { throw ("record 0x{0:x} has zero uid" -f $record) }
    if ($count -le 0) { throw ("record 0x{0:x} has invalid count {1}" -f $record, $count) }
    if ($modCount -ne 0 -and $mods -eq 0) {
        throw ("record 0x{0:x} has mod count {1} but null mod array" -f $record, $modCount)
    }

    $itemLine = [string]::Format(
        [System.Globalization.CultureInfo]::InvariantCulture,
        "item|{0}|{1}|{2}|0x{3:x}|{4}",
        $person, $itemId, $count, $flags, $modCount)
    [void]$out.AppendLine($itemLine)

    for ($index = 0; $index -lt $modCount; $index++) {
        $mod = [uint64]$mods + ([uint64]$index * 0x14)
        $modId = Read-U32 $mod
        $value = Read-I32 ($mod + 4)
        $modFlags = Read-U32 ($mod + 8)
        $line = [string]::Format(
            [System.Globalization.CultureInfo]::InvariantCulture,
            "mod|{0}|{1}|{2}|0x{3:x}|{4}|{5}|0x{6:x}",
            $person, $itemId, $count, $flags, $modId, $value, $modFlags)
        [void]$out.AppendLine($line)
    }
}

$output = [System.Text.StringBuilder]::new()
[void]$output.AppendLine("# gl-specials-live-capture v2")
[void]$output.AppendLine("format=gl-specials-live-2")
[void]$output.AppendLine("level=$Level")
[void]$output.AppendLine("seed=$Seed")
[void]$output.AppendLine(("rng_state=0x{0:x8}" -f $RngState))
[void]$output.AppendLine("loot_level=$LootLevel")

try {
    foreach ($target in (Convert-Targets $NpcContainer)) {
        if ($managerAddress -eq 0) {
            throw "Container mode needs the Server.dll base or -Manager (value of DAT_14088df8)"
        }
        $container = $target.Address
        $count = Read-U32 ($container + 0x10)
        $ids = Read-U32 ($container + 0x0c)
        $bitmap = Read-U32 ($container + 0x14)
        $recordPointers = Read-U32 ($managerAddress + 0x18)
        $managerCount = Read-U32 ($managerAddress + 0x1c)
        $managerBitmap = Read-U32 ($managerAddress + 0x20)
        for ($slot = 0; $slot -lt $count; $slot++) {
            $used = (Read-Bytes ($bitmap + [uint64]($slot -shr 3)) 1)[0] -band (1 -shl ($slot -band 7))
            if ($used -eq 0) { continue }
            $recordId = Read-U32 ($ids + [uint64]($slot * 4))
            if ($recordId -ge $managerCount) {
                throw ("record id {0} >= manager count {1}" -f $recordId, $managerCount)
            }
            $recordUsed = (Read-Bytes ($managerBitmap + [uint64]($recordId -shr 3)) 1)[0] -band
                (1 -shl ($recordId -band 7))
            if ($recordUsed -eq 0) { throw ("record id {0} is not marked used" -f $recordId) }
            $record = Read-U32 ($recordPointers + [uint64]($recordId * 4))
            if ($record -eq 0) { throw ("null item record for slot {0} id {1}" -f $slot, $recordId) }
            Add-ItemRecord $output $target.Person $record
        }
    }

    foreach ($target in (Convert-Targets $Item)) {
        Add-ItemRecord $output $target.Person $target.Address
    }
} finally {
    [void][GLSpecialsMem]::CloseHandle($handle)
}

$encoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($OutFile, $output.ToString(), $encoding)
Write-Output ("wrote {0}" -f $OutFile)
