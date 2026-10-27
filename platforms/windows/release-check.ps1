<#
.SYNOPSIS
  Pre-release check of the qlcconsole Windows installer: installs it silently
  into a temp folder, checks the result, uninstalls it and checks that
  nothing is left behind.

.DESCRIPTION
  powershell -ExecutionPolicy Bypass -File platforms\windows\release-check.ps1 <setup.exe>
      [-Version 0.3.0] [-Rev 38e1963b8] [-WorkDir <dir>] [-OffscreenDir <dir>]
      [-Waive id,id] [-SkipTakeover]

  Checks, in order:
    - the installer's Authenticode signature (unsigned is reported, as the
      expected state for now, not failed) and its version resource;
    - a silent install (/S /D=<temp>): exit code, the expected files, every
      DLL each EXE/DLL imports resolvable from the install folder or Windows
      (a PE import walker, no dumpbin), all x64;
    - --version and a headless load of the bundled Sample.qxw, both with
      -platform offscreen (an offscreen plugin is supplied from -OffscreenDir
      when the installer ships none, and removed again before the uninstall);
    - the registry: the Apps & features entry (name, version, publisher,
      InstallLocation, UninstallString), the .qlcc/.qxw/.qxf associations
      (ProgID, icon, open command and its quoting), the Start Menu shortcuts'
      targets;
    - FFmpeg's licence: its configure line (GPL or LGPL) and whether the
      matching licence text ships;
    - uninstall /S, then that NOTHING remains: files, the uninstall entry,
      associations, shortcuts, HKCU\Software\qlcconsole and the app's
      settings, HKCU\Software\qlcplus\qlcconsole;
    - (unless -SkipTakeover) a second install into another folder must not
      take over the first one's Apps & features entry, associations and
      shortcuts.

  Every registry key and Start Menu folder it can touch is exported first and
  restored at the end, exactly, whatever happens -- a qlcconsole already
  installed on the machine keeps its entry, associations, shortcuts and
  settings. The app is only ever run with -platform offscreen and its own
  QSettings (QLC_HELP_SHOTS), never with a window. Crash dumps it causes are
  removed (and fail the check).

  Output: TAP on stdout and <WorkDir>\out\release-check.tap, JSON in
  release-check.json, a summary at the end. Exit 0 = all passed (waived
  aside), 1 = a check failed, 2 = could not run.
#>
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Setup,
    [string]$Version = "",
    [string]$Rev = "",
    [string]$WorkDir = "",
    [string]$OffscreenDir = "",
    [string[]]$Waive = @(),
    [switch]$SkipTakeover
)

$ErrorActionPreference = "Continue"
Set-StrictMode -Version 2

if (-not (Test-Path -LiteralPath $Setup -PathType Leaf)) { Write-Host "release-check: no such installer: $Setup"; exit 2 }
$Setup = (Resolve-Path -LiteralPath $Setup).Path
$SetupName = Split-Path -Leaf $Setup
# qlcconsole-<version>[-<date>-<rev>]-win64-setup.exe
if (-not $Version -and $SetupName -match '^qlcconsole-(\d+\.\d+\.\d+)') { $Version = $Matches[1] }
if (-not $Rev -and $SetupName -match '-([0-9a-f]{7,40})-win64-setup\.exe$') { $Rev = $Matches[1] }
if ($env:RELEASE_CHECK_WAIVE) { $Waive += $env:RELEASE_CHECK_WAIVE -split ',' }

if (-not $WorkDir) { $WorkDir = Join-Path ([IO.Path]::GetTempPath()) ("qlc-relcheck-" + [Guid]::NewGuid().ToString("N").Substring(0, 8)) }
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$WorkDir = (Resolve-Path -LiteralPath $WorkDir).Path
$Out = Join-Path $WorkDir "out"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$TapFile = Join-Path $Out "release-check.tap"
$JsonFile = Join-Path $Out "release-check.json"
Set-Content -LiteralPath $TapFile -Value $null -Encoding UTF8

# --- TAP / JSON --------------------------------------------------------------
$script:N = 0; $script:Fails = @(); $script:Waived = @(); $script:Skips = 0
$script:Json = New-Object System.Collections.ArrayList
function Emit([string]$line) { Write-Host $line; Add-Content -LiteralPath $TapFile -Value $line -Encoding UTF8 }
function Rec([string]$status, [string]$id, [string]$detail) {
    [void]$script:Json.Add([ordered]@{ id = $id; status = $status; detail = $detail })
}
function Pass([string]$id, [string]$detail = "") {
    $script:N++; $d = ""; if ($detail) { $d = " # $detail" }
    Emit "ok $($script:N) - $id$d"; Rec "pass" $id $detail
}
function Fail([string]$id, [string]$detail = "") {
    $script:N++
    if ($Waive -contains $id) {
        $script:Waived += "$id`: $detail"; Emit "not ok $($script:N) - $id # TODO waived: $detail"; Rec "waived" $id $detail
    } else {
        $script:Fails += "$id`: $detail"; $d = ""; if ($detail) { $d = " # $detail" }
        Emit "not ok $($script:N) - $id$d"; Rec "fail" $id $detail
    }
}
function Skip([string]$id, [string]$why) { $script:N++; $script:Skips++; Emit "ok $($script:N) - $id # SKIP $why"; Rec "skip" $id $why }
function Note([string]$s) { Emit "# $s" }
function Section([string]$s) { Emit "# --- $s ---" }
function Check([string]$id, [bool]$ok, [string]$detail) { if ($ok) { Pass $id } else { Fail $id $detail } }

# --- PE reader -------------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
public static class QlcPe {
    // Machine type and imported DLL names (normal + delay-load) of a PE file;
    // null when it is not one.
    public static string[] Read(string path, out ushort machine) {
        machine = 0;
        byte[] b;
        try { b = File.ReadAllBytes(path); } catch { return null; }
        if (b.Length < 0x40 || b[0] != 'M' || b[1] != 'Z') return null;
        int pe = BitConverter.ToInt32(b, 0x3c);
        if (pe <= 0 || pe + 24 > b.Length || BitConverter.ToUInt32(b, pe) != 0x4550) return null;
        machine = BitConverter.ToUInt16(b, pe + 4);
        int nsec = BitConverter.ToUInt16(b, pe + 6);
        int optSize = BitConverter.ToUInt16(b, pe + 20);
        int opt = pe + 24;
        ushort magic = BitConverter.ToUInt16(b, opt);
        int dd = opt + (magic == 0x20b ? 112 : 96);
        int sec = opt + optSize;
        Func<uint, int> off = rva => {
            for (int i = 0; i < nsec; i++) {
                int s = sec + i * 40;
                uint va = BitConverter.ToUInt32(b, s + 12), vs = BitConverter.ToUInt32(b, s + 8);
                uint raw = BitConverter.ToUInt32(b, s + 20), rs = BitConverter.ToUInt32(b, s + 16);
                uint size = Math.Max(vs, rs);
                if (rva >= va && rva < va + size) return (int)(rva - va + raw);
            }
            return -1;
        };
        Func<int, string> cstr = o => {
            if (o < 0 || o >= b.Length) return null;
            int e = o; while (e < b.Length && b[e] != 0) e++;
            return Encoding.ASCII.GetString(b, o, e - o);
        };
        var names = new List<string>();
        uint imp = BitConverter.ToUInt32(b, dd + 8);
        if (imp != 0) {
            int o = off(imp);
            while (o > 0 && o + 20 <= b.Length) {
                uint nameRva = BitConverter.ToUInt32(b, o + 12);
                if (nameRva == 0) break;
                string n = cstr(off(nameRva)); if (n != null) names.Add(n);
                o += 20;
            }
        }
        uint dly = BitConverter.ToUInt32(b, dd + 13 * 8);
        if (dly != 0) {
            int o = off(dly);
            while (o > 0 && o + 32 <= b.Length) {
                uint nameRva = BitConverter.ToUInt32(b, o + 4);
                if (nameRva == 0) break;
                string n = cstr(off(nameRva)); if (n != null) names.Add(n);
                o += 32;
            }
        }
        return names.ToArray();
    }
    // Whether the file holds an ASCII string (FFmpeg's configure line, ...).
    public static string FindAscii(string path, string needle, int max) {
        byte[] b = File.ReadAllBytes(path);
        byte[] n = Encoding.ASCII.GetBytes(needle);
        for (int i = 0; i + n.Length <= b.Length; i++) {
            int j = 0; while (j < n.Length && b[i + j] == n[j]) j++;
            if (j == n.Length) {
                int s = i; while (s > 0 && b[s - 1] >= 0x20 && b[s - 1] < 0x7f) s--;
                int e = i; while (e < b.Length && b[e] >= 0x20 && b[e] < 0x7f && e - s < max) e++;
                return Encoding.ASCII.GetString(b, s, e - s);
            }
        }
        return null;
    }
}
"@

# --- state we may touch: exported first, restored at the end -------------------
$Keys = @(
    'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\qlcconsole',
    'HKCU\Software\Classes\.qlcc', 'HKCU\Software\Classes\.qxw', 'HKCU\Software\Classes\.qxf',
    'HKCU\Software\Classes\QLCConsole.Document', 'HKCU\Software\Classes\QLCConsoleFixture.Document',
    'HKCU\Software\qlcconsole', 'HKCU\Software\qlcplus\qlcconsole'
)
$Programs = [Environment]::GetFolderPath('Programs')
$SmDirs = @((Join-Path $Programs 'qlcconsole'))
$SmLoose = @('qlcconsole.lnk', 'Fixture Definition Editor.lnk', 'Uninstall.lnk') | ForEach-Object { Join-Path $Programs $_ }
$Backup = Join-Path $WorkDir "backup"
New-Item -ItemType Directory -Force -Path $Backup | Out-Null

function KeyExists([string]$k) { & reg.exe query $k *> $null; return ($LASTEXITCODE -eq 0) }
function PsKey([string]$k) { return "Registry::" + ($k -replace '^HKCU', 'HKEY_CURRENT_USER') }
function RegVal([string]$k, [string]$name) {
    $p = Get-ItemProperty -LiteralPath (PsKey $k) -ErrorAction SilentlyContinue
    if ($null -eq $p) { return $null }
    if ($name -eq '') { $name = '(default)' }
    if ($p.PSObject.Properties.Name -contains $name) { return [string]$p.$name }
    return $null
}

$i = 0
foreach ($k in $Keys) {
    $i++
    if (KeyExists $k) { & reg.exe export $k (Join-Path $Backup "key$i.reg") /y *> $null }
}
$j = 0
foreach ($d in $SmDirs) { $j++; if (Test-Path -LiteralPath $d) { Copy-Item -LiteralPath $d -Destination (Join-Path $Backup "sm$j") -Recurse -Force } }
New-Item -ItemType Directory -Force -Path (Join-Path $Backup "smloose") | Out-Null
# What every Start Menu item looks like now, to restore and then verify.
$SmState = @{}
function Sm-Items { foreach ($d in $SmDirs) { if (Test-Path -LiteralPath $d) { Get-ChildItem -LiteralPath $d -Recurse -File } }; foreach ($f in $SmLoose) { if (Test-Path -LiteralPath $f) { Get-Item -LiteralPath $f } } }
foreach ($it in @(Sm-Items)) { $SmState[$it.FullName] = @((Get-FileHash -LiteralPath $it.FullName).Hash, $it.LastWriteTimeUtc) }
foreach ($f in $SmLoose) { if (Test-Path -LiteralPath $f) { Copy-Item -LiteralPath $f -Destination (Join-Path $Backup "smloose") -Force } }
$KeyState = @{}
foreach ($k in $Keys) { if (KeyExists $k) { $KeyState[$k] = (& reg.exe query $k /s | Out-String) } }
$CrashDirs = @((Join-Path $env:LOCALAPPDATA 'CrashDumps'), (Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\WER\ReportArchive'), (Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\WER\ReportQueue'))
$CrashBefore = @{}
foreach ($d in $CrashDirs) { if (Test-Path $d) { Get-ChildItem -LiteralPath $d -ErrorAction SilentlyContinue | ForEach-Object { $CrashBefore[$_.FullName] = 1 } } }

function Restore-State {
    $i = 0
    foreach ($k in $Keys) {
        $i++
        if (KeyExists $k) { & reg.exe delete $k /f *> $null }
        $f = Join-Path $Backup "key$i.reg"
        if (Test-Path -LiteralPath $f) { & reg.exe import $f *> $null }
    }
    $j = 0
    foreach ($d in $SmDirs) {
        $j++
        if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction SilentlyContinue }
        $b = Join-Path $Backup "sm$j"
        if (Test-Path -LiteralPath $b) { Copy-Item -LiteralPath $b -Destination $d -Recurse -Force }
    }
    foreach ($f in $SmLoose) {
        $b = Join-Path (Join-Path $Backup "smloose") (Split-Path -Leaf $f)
        if (Test-Path -LiteralPath $b) {
            $same = (Test-Path -LiteralPath $f) -and ((Get-FileHash -LiteralPath $f).Hash -eq (Get-FileHash -LiteralPath $b).Hash)
            if (-not $same) { Copy-Item -LiteralPath $b -Destination $f -Force }
        }
        elseif (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
    }
    # Times as they were (a copy or a shell refresh changes them).
    foreach ($p in $SmState.Keys) { if (Test-Path -LiteralPath $p) { (Get-Item -LiteralPath $p).LastWriteTimeUtc = $SmState[$p][1] } }
}
function Restored-Differences {
    $diff = @()
    foreach ($k in $Keys) {
        $now = $null; if (KeyExists $k) { $now = (& reg.exe query $k /s | Out-String) }
        $was = $null; if ($KeyState.ContainsKey($k)) { $was = $KeyState[$k] }
        if ($now -ne $was) { $diff += $k }
    }
    $items = @(Sm-Items)
    foreach ($it in $items) {
        if (-not $SmState.ContainsKey($it.FullName)) { $diff += "$($it.FullName) (new)"; continue }
        $w = $SmState[$it.FullName]
        if ((Get-FileHash -LiteralPath $it.FullName).Hash -ne $w[0] -or $it.LastWriteTimeUtc -ne $w[1]) { $diff += "$($it.FullName) (changed)" }
    }
    foreach ($p in $SmState.Keys) { if (-not (Test-Path -LiteralPath $p)) { $diff += "$p (gone)" } }
    return $diff
}

# --- install / uninstall helpers ------------------------------------------------
function Install-To([string]$dir) {
    # /D= must be last and unquoted, even with spaces.
    $p = Start-Process -FilePath $Setup -ArgumentList @('/S', "/D=$dir") -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit(900000)) { try { $p.Kill() } catch {}; return -1 }
    return $p.ExitCode
}
function Wait-Uninstaller {
    # NSIS's uninstaller copies itself to %TEMP%\~nsu*.tmp and returns at
    # once: wait for that copy to finish.
    $deadline = (Get-Date).AddSeconds(300)
    Start-Sleep -Seconds 2
    while ((Get-Date) -lt $deadline) {
        $busy = Get-Process -ErrorAction SilentlyContinue | Where-Object {
            try { $_.Path -and ($_.Path -like '*\~nsu*' -or $_.Path -like '*\Un_*.exe' -or $_.Path -like '*\Au_*.exe') } catch { $false } }
        if (-not $busy) { Start-Sleep -Seconds 1; return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}
function Uninstall-From([string]$dir) {
    $u = Join-Path $dir 'uninstall.exe'
    if (-not (Test-Path -LiteralPath $u)) { return -2 }
    $p = Start-Process -FilePath $u -ArgumentList @('/S') -PassThru -WindowStyle Hidden
    $p.WaitForExit(300000) | Out-Null
    $rc = $p.ExitCode
    if (-not (Wait-Uninstaller)) { return -1 }
    return $rc
}
function Run-App([string]$exe, [string[]]$appArgs, [int]$timeoutS, [string]$log) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = (($appArgs | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join ' ')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true; $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $script:RunDir
    $home2 = Join-Path $script:RunDir 'home'
    New-Item -ItemType Directory -Force -Path $home2 | Out-Null
    $psi.EnvironmentVariables['QT_QPA_PLATFORM'] = 'offscreen'
    $psi.EnvironmentVariables['QLC_HELP_SHOTS'] = (Join-Path $script:RunDir 'shots')
    $psi.EnvironmentVariables['QLC_HELP_SHOTS_ONLY'] = '__relcheck_none__'
    $psi.EnvironmentVariables['USERPROFILE'] = $home2
    $psi.EnvironmentVariables['HOME'] = $home2
    $psi.EnvironmentVariables['APPDATA'] = (Join-Path $home2 'AppData\Roaming')
    $psi.EnvironmentVariables['LOCALAPPDATA'] = (Join-Path $home2 'AppData\Local')
    $p = [System.Diagnostics.Process]::Start($psi)
    $so = $p.StandardOutput.ReadToEndAsync(); $se = $p.StandardError.ReadToEndAsync()
    $rc = 0
    if (-not $p.WaitForExit($timeoutS * 1000)) { try { $p.Kill() } catch {}; $rc = -1 } else { $p.WaitForExit(); $rc = $p.ExitCode }
    Set-Content -LiteralPath $log -Value ($so.Result + "`n" + $se.Result) -Encoding UTF8
    return $rc
}

$SysDirs = @((Join-Path $env:windir 'System32'), $env:windir)
function Resolve-Dll([string]$name, [string]$appDir) {
    if ($name -match '^(api|ext)-ms-') { return $true }       # API sets
    foreach ($d in @($appDir) + $SysDirs) { if (Test-Path -LiteralPath (Join-Path $d $name)) { return $true } }
    return $false
}

$script:RunDir = Join-Path $WorkDir 'run'
New-Item -ItemType Directory -Force -Path $script:RunDir | Out-Null
$InstA = Join-Path $WorkDir 'inst-a'
$InstB = Join-Path $WorkDir 'inst-b'
$Uk = 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\qlcconsole'
$Exit = 0

try {
    Note "qlcconsole Windows release check: $SetupName"
    Note "host $env:COMPUTERNAME $([Environment]::OSVersion.VersionString); expect version=$Version rev=$Rev"
    $prior = RegVal $Uk 'InstallLocation'
    if ($prior) {
        # The installer refuses a second install into another folder (see the
        # takeover check): hide the real one's entry for the duration. It was
        # exported above and is restored at the end.
        Note "a qlcconsole is already installed at $prior (its registration is exported, hidden for the check and restored)"
        & reg.exe delete $Uk /f *> $null
    }

    # =========================================================================
    Section "Installer"
    $sig = Get-AuthenticodeSignature -LiteralPath $Setup
    switch ([string]$sig.Status) {
        'Valid' { Pass 'win.authenticode' "signed: $($sig.SignerCertificate.Subject)" }
        'NotSigned' { Skip 'win.authenticode' 'installer is NOT Authenticode-signed (expected for now: SmartScreen warns on first run)' }
        default { Fail 'win.authenticode' "signature status $($sig.Status): $($sig.StatusMessage)" }
    }
    $vi = (Get-Item -LiteralPath $Setup).VersionInfo
    Check 'win.installer-version' ($vi.ProductVersion -eq $Version) "ProductVersion '$($vi.ProductVersion)', want $Version"

    # =========================================================================
    Section "Silent install (/S /D=$InstA)"
    $rc = Install-To $InstA
    Check 'win.install' ($rc -eq 0 -and (Test-Path -LiteralPath (Join-Path $InstA 'qlcconsole.exe'))) "installer exit $rc"
    if (-not (Test-Path -LiteralPath (Join-Path $InstA 'qlcconsole.exe'))) { throw "nothing installed; cannot go on" }

    $must = @('qlcconsole.exe', 'qlcconsole-fixtureeditor.exe', 'uninstall.exe', 'qlcplusengine.dll', 'qlcplusui.dll',
              'qlcpluswebaccess.dll', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll',
              'imageformats\qjpeg.dll', 'imageformats\qsvg.dll', 'imageformats\qico.dll', 'iconengines\qsvgicon.dll',
              'Sample.qxw', 'Fixtures\FixturesMap.xml')
    $missing = $must | Where-Object { -not (Test-Path -LiteralPath (Join-Path $InstA $_)) }
    Check 'win.files' (-not $missing) "missing: $($missing -join ', ')"
    $styles = @(Get-ChildItem -LiteralPath (Join-Path $InstA 'styles') -Filter *.dll -ErrorAction SilentlyContinue)
    Note "styles: $(($styles | ForEach-Object Name) -join ' ')"
    $mm = @(Get-ChildItem -LiteralPath (Join-Path $InstA 'multimedia') -Filter *.dll -ErrorAction SilentlyContinue)
    Check 'win.plugin.multimedia' ($mm.Count -gt 0) "no multimedia backend"
    $ioDir = Join-Path $InstA 'Plugins'
    $io = @(Get-ChildItem -LiteralPath $ioDir -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object { $_.BaseName })
    $wantIo = @('artnet', 'dmxusb', 'e131', 'enttecwing', 'hidplugin', 'loopback', 'midiplugin', 'os2l', 'osc', 'udmx', 'wled')
    $missIo = $wantIo | Where-Object { $io -notcontains $_ }
    Check 'win.plugin.io' (-not $missIo) "missing I/O plugins: $($missIo -join ', ') (have: $($io -join ' '))"
    $langs = @('de_DE', 'es_ES', 'fr_FR', 'it_IT', 'nl_NL', 'cz_CZ', 'pt_BR', 'ca_ES', 'ja_JP')
    $missL = $langs | Where-Object { -not (Get-ChildItem -LiteralPath $InstA -Recurse -Filter "qlcconsole_$_.qm" -ErrorAction SilentlyContinue) }
    Check 'win.translations' (-not $missL) "missing qlcconsole_<lang>.qm: $($missL -join ' ')"
    foreach ($d in @('RGBScripts', 'EffectScripts', 'InputProfiles', 'Web', 'Gobos')) {
        # (Not $n: PowerShell names are case-blind, and $N counts the checks.)
        $cnt = @(Get-ChildItem -LiteralPath (Join-Path $InstA $d) -Recurse -File -ErrorAction SilentlyContinue).Count
        Check "win.dir.$d" ($cnt -gt 0) "$d missing or empty"
    }
    $dups = @(Get-ChildItem -LiteralPath $InstA -Recurse -File | Where-Object { $_.Name -match ' (\d+|copy)\.[^.]+$' })
    Check 'win.no-duplicates' ($dups.Count -eq 0) "$($dups.Count) duplicate-named files: $(($dups | Select-Object -First 5 | ForEach-Object Name) -join '; ')"

    # Every import of every EXE/DLL resolves; all x64.
    $unres = New-Object System.Collections.ArrayList; $notx64 = @(); $npe = 0
    foreach ($f in Get-ChildItem -LiteralPath $InstA -Recurse -File -Include *.exe, *.dll) {
        $m = [uint16]0
        $imps = [QlcPe]::Read($f.FullName, [ref]$m)
        if ($null -eq $imps) { continue }
        $npe++
        if ($m -ne 0x8664) { $notx64 += $f.FullName.Substring($InstA.Length + 1) }
        foreach ($d in $imps) {
            if (-not (Resolve-Dll $d $InstA)) { [void]$unres.Add("$($f.FullName.Substring($InstA.Length + 1)) -> $d") }
        }
    }
    Note "$npe PE files"
    Check 'win.pe.x64' ($notx64.Count -eq 0) "not x64: $($notx64 -join ', ')"
    Check 'win.pe.imports-resolve' ($unres.Count -eq 0) "unresolved: $(($unres | Select-Object -First 12) -join '; ')"

    # =========================================================================
    Section "FFmpeg licence"
    $avc = Get-ChildItem -LiteralPath $InstA -Filter 'avcodec-*.dll' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($avc) {
        $conf = [QlcPe]::FindAscii($avc.FullName, '--prefix=', 4000)
        if (-not $conf) { $conf = [QlcPe]::FindAscii($avc.FullName, '--enable-', 4000) }
        $gpl = $conf -and ($conf -match '--enable-gpl')
        $v3 = $conf -and ($conf -match '--enable-version3')
        $nonfree = $conf -and ($conf -match '--enable-nonfree')
        $kind = 'LGPL'; if ($gpl) { $kind = 'GPL' }; if ($v3) { $kind += 'v3' } else { $kind += 'v2.1+' }
        if ($nonfree) { $kind = 'NONFREE (not redistributable)' }
        Note "$($avc.Name): $kind; configure: $(if ($conf) { $conf.Substring(0, [Math]::Min(400, $conf.Length)) } else { 'not found' })"
        $want = 'GNU LESSER GENERAL PUBLIC LICENSE'; if ($gpl) { $want = 'GNU GENERAL PUBLIC LICENSE' }
        $texts = @(Get-ChildItem -LiteralPath $InstA -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -lt 200000 -and $_.Extension -match '^\.(txt|md|html?|)$' } |
            Where-Object { Select-String -LiteralPath $_.FullName -SimpleMatch $want -Quiet })
        if ($nonfree) { Fail 'win.ffmpeg.licence' "FFmpeg built --enable-nonfree" }
        elseif ($gpl) { Fail 'win.ffmpeg.licence' "FFmpeg is built --enable-gpl ($kind): the installer is Apache-2.0 and the macOS DMG ships LGPL FFmpeg" }
        else { Pass 'win.ffmpeg.licence' $kind }
        Check 'win.ffmpeg.licence-text' ($texts.Count -gt 0) "no '$want' text ships with the $kind FFmpeg"
    } else {
        Skip 'win.ffmpeg.licence' 'no avcodec DLL'
    }

    # =========================================================================
    Section "Registry and Start Menu after install"
    Check 'win.reg.uninstall-key' (KeyExists $Uk) "no $Uk"
    $dn = RegVal $Uk 'DisplayName'; Check 'win.reg.display-name' ($dn -eq 'qlcconsole') "DisplayName '$dn'"
    $dv = RegVal $Uk 'DisplayVersion'; Check 'win.reg.display-version' ($dv -eq $Version) "DisplayVersion '$dv', want $Version"
    $pub = RegVal $Uk 'Publisher'; Check 'win.reg.publisher' ([bool]$pub) "no Publisher in the Apps & features entry"
    $il = RegVal $Uk 'InstallLocation'; Check 'win.reg.install-location' ($il -eq $InstA) "InstallLocation '$il', want $InstA"
    $us = RegVal $Uk 'UninstallString'
    Check 'win.reg.uninstall-string' ($us -eq ('"' + (Join-Path $InstA 'uninstall.exe') + '"')) "UninstallString '$us' (must be the quoted path of uninstall.exe)"
    $di = RegVal $Uk 'DisplayIcon'
    Check 'win.reg.display-icon' ($di -and (Test-Path -LiteralPath ($di -replace ',\d+$', ''))) "DisplayIcon '$di'"
    $assoc = @{ 'qlcc' = 'qlcconsole.exe'; 'qxw' = 'qlcconsole.exe'; 'qxf' = 'qlcconsole-fixtureeditor.exe' }
    foreach ($e in $assoc.Keys) {
        $prog = RegVal "HKCU\Software\Classes\.$e" ''
        if (-not $prog) { Fail "win.assoc.$e" "HKCU\Software\Classes\.$e has no ProgID"; continue }
        $exe = Join-Path $InstA $assoc[$e]
        $icon = RegVal "HKCU\Software\Classes\$prog\DefaultIcon" ''
        $cmd = RegVal "HKCU\Software\Classes\$prog\shell\open\command" ''
        $why = @()
        if ($icon -ne "$exe,0") { $why += "DefaultIcon '$icon'" }
        if ($cmd -ne ('"' + $exe + '" --open "%1"')) { $why += "open command '$cmd' (want `"$exe`" --open `"%1`")" }
        if ($why) { Fail "win.assoc.$e" "$prog`: $($why -join '; ')" } else { Pass "win.assoc.$e" "$prog -> $($assoc[$e]) --open `"%1`"" }
    }
    $smDir = RegVal 'HKCU\Software\qlcconsole' 'StartMenu_Dir'
    $wsh = New-Object -ComObject WScript.Shell
    $lnks = @{ 'qlcconsole.lnk' = 'qlcconsole.exe'; 'Fixture Definition Editor.lnk' = 'qlcconsole-fixtureeditor.exe'; 'Uninstall.lnk' = 'uninstall.exe' }
    if ($smDir -and (Test-Path -LiteralPath $smDir)) {
        foreach ($l in $lnks.Keys) {
            $p = Join-Path $smDir $l
            if (-not (Test-Path -LiteralPath $p)) { Fail "win.shortcut.$l" "missing $p"; continue }
            $t = $wsh.CreateShortcut($p).TargetPath
            Check "win.shortcut.$l" ($t -eq (Join-Path $InstA $lnks[$l])) "points at '$t'"
        }
    } else {
        Fail 'win.shortcuts' "no Start Menu folder (StartMenu_Dir '$smDir')"
    }
    $id = RegVal 'HKCU\Software\qlcconsole' 'Install_Dir'
    Check 'win.reg.install-dir' ($id -eq $InstA) "HKCU\Software\qlcconsole Install_Dir '$id'"

    # =========================================================================
    Section "Headless runs (-platform offscreen, own settings)"
    $added = @()
    $offscreen = Join-Path $InstA 'platforms\qoffscreen.dll'
    if (Test-Path -LiteralPath $offscreen) {
        Pass 'win.plugin.offscreen' 'shipped'
    } else {
        Fail 'win.plugin.offscreen' 'no platforms\qoffscreen.dll: the app cannot run headless (-platform offscreen) as shipped'
        $src = if ($OffscreenDir) { Join-Path $OffscreenDir 'platforms\qoffscreen.dll' } else { '' }
        if ($src -and (Test-Path -LiteralPath $src)) {
            Copy-Item -LiteralPath $src -Destination $offscreen; $added += $offscreen
            # ... and whatever it imports that the installer lacks.
            $m = [uint16]0
            foreach ($d in [QlcPe]::Read($offscreen, [ref]$m)) {
                if (-not (Resolve-Dll $d $InstA)) {
                    $s = Join-Path $OffscreenDir $d
                    if (Test-Path -LiteralPath $s) { Copy-Item -LiteralPath $s -Destination $InstA; $added += (Join-Path $InstA $d) }
                }
            }
            Note "supplied for the runs (removed before the uninstall): $(($added | ForEach-Object { $_.Substring($InstA.Length + 1) }) -join ', ')"
        }
    }
    $exe = Join-Path $InstA 'qlcconsole.exe'
    if (Test-Path -LiteralPath $offscreen) {
        $log = Join-Path $Out 'version.log'
        $rc = Run-App $exe @('--version', '-platform', 'offscreen') 120 $log
        $line = (Get-Content -LiteralPath $log | Where-Object { $_ -match '^qlcconsole ' } | Select-Object -First 1)
        $ok = ($rc -eq 0 -and $line -and $line.StartsWith("qlcconsole $Version (build ") -and (-not $Rev -or $line -match ", $Rev"))
        Check 'win.run.version' $ok "exit $rc, '$line' (want $Version, $Rev)"
        if ($ok) { Note $line }
        $fe = Join-Path $InstA 'qlcconsole-fixtureeditor.exe'
        $rc = Run-App $fe @('--version', '-platform', 'offscreen') 120 (Join-Path $Out 'fe-version.log')
        Check 'win.run.fixtureeditor-version' ($rc -eq 0) "exit $rc"

        Copy-Item -LiteralPath (Join-Path $InstA 'Sample.qxw') -Destination (Join-Path $script:RunDir 'Sample.qxw') -Force
        $log = Join-Path $Out 'load-sample.log'
        $rc = Run-App $exe @('-platform', 'offscreen', '-d', '0', '-o', (Join-Path $script:RunDir 'Sample.qxw')) 300 $log
        $txt = Get-Content -LiteralPath $log -Raw
        $ok = ($rc -eq 0 -and $txt -match '\[HELPSHOT\] done, 0 failed' -and $txt -match 'addWidgetInMap')
        Check 'win.run.load-sample' $ok "exit $rc; see $log"
        $loaded = @([regex]::Matches($txt, 'Loaded I/O plugin "[^"]*" from "([^"]+)"') | ForEach-Object { $_.Groups[1].Value -replace '\.dll$', '' })
        $notLoaded = $io | Where-Object { $loaded -notcontains $_ }
        Check 'win.run.io-plugins-loaded' (-not $notLoaded) "not loaded: $($notLoaded -join ', ')"
        $ff = ($txt -split "`n" | Where-Object { $_ -match 'FFmpeg version' } | Select-Object -First 1)
        # QtMultimedia's FFmpeg backend loaded (it logs its FFmpeg build at -d 0).
        Check 'win.run.multimedia' ([bool]$ff) "the FFmpeg multimedia backend did not load (see $log)"
        if ($ff) { Note $ff.Trim() }
    } else {
        Skip 'win.run.version' 'no offscreen plugin (-OffscreenDir)'
        Skip 'win.run.load-sample' 'no offscreen plugin (-OffscreenDir)'
    }
    foreach ($f in $added) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }

    # A real user's run leaves its settings: make sure there are some for the
    # uninstaller to clear (the runs above used their own).
    $settings = 'HKCU\Software\qlcplus\qlcconsole'
    if (-not (KeyExists $settings)) {
        & reg.exe add $settings /v relcheck /t REG_SZ /d 'settings a run of the app leaves' /f *> $null
        Note "seeded $settings (as the app's first run would)"
    }

    # =========================================================================
    Section "Uninstall (/S) leaves nothing"
    $rc = Uninstall-From $InstA
    Check 'win.uninstall' ($rc -eq 0) "uninstaller exit $rc"
    $left = @()
    if (Test-Path -LiteralPath $InstA) { $left = @(Get-ChildItem -LiteralPath $InstA -Recurse -Force | ForEach-Object { $_.FullName.Substring($InstA.Length + 1) }) }
    Check 'win.clean.files' (-not (Test-Path -LiteralPath $InstA)) "install folder still there$(if ($left) { ': ' + (($left | Select-Object -First 8) -join ', ') })"
    Check 'win.clean.uninstall-key' (-not (KeyExists $Uk)) "$Uk remains"
    foreach ($k in @('HKCU\Software\Classes\.qlcc', 'HKCU\Software\Classes\.qxw', 'HKCU\Software\Classes\.qxf',
                     'HKCU\Software\Classes\QLCConsole.Document', 'HKCU\Software\Classes\QLCConsoleFixture.Document')) {
        Check "win.clean.$((Split-Path -Leaf $k).TrimStart('.'))" (-not (KeyExists $k)) "$k remains"
    }
    $smLeft = @()
    if ($smDir -and (Test-Path -LiteralPath $smDir)) { $smLeft += $smDir }
    Check 'win.clean.shortcuts' ($smLeft.Count -eq 0) "Start Menu folder remains: $($smLeft -join ', ')"
    Check 'win.clean.installer-key' (-not (KeyExists 'HKCU\Software\qlcconsole')) 'HKCU\Software\qlcconsole remains'
    Check 'win.clean.settings' (-not (KeyExists $settings)) "$settings (the app's QSettings) remains: the uninstaller deletes HKCU\Software\qlcconsole, not the key the app writes"

    # =========================================================================
    if ($SkipTakeover) {
        Skip 'win.takeover' '-SkipTakeover'
    } else {
        Section "A second install into another folder"
        $rcA = Install-To $InstA; $rcB = Install-To $InstB
        $bExe = Test-Path -LiteralPath (Join-Path $InstB 'qlcconsole.exe')
        if ($rcA -ne 0) {
            Fail 'win.takeover' "the first install failed ($rcA)"
        } elseif ($rcB -ne 0 -and $bExe) {
            Fail 'win.takeover' "the second install failed ($rcB) but left files in $InstB"
        } elseif ($rcB -ne 0 -and -not ((RegVal $Uk 'InstallLocation') -eq $InstA)) {
            Fail 'win.takeover' "the second install was refused ($rcB) but the first one's entry is gone or changed"
        } elseif ($rcB -ne 0) {
            # The answer we want in a silent install: refuse (exit 3), touch nothing.
            Pass 'win.takeover' "a silent install into another folder is refused (exit $rcB); the first install is untouched"
        } else {
            $why = @()
            $il = RegVal $Uk 'InstallLocation'
            if ($il -ne $InstA) { $why += "the Apps & features entry now points at '$il'" }
            $cmd = RegVal 'HKCU\Software\Classes\QLCConsole.Document\shell\open\command' ''
            if ($cmd -notlike "*$InstA\*") { $why += ".qlcc/.qxw now open with '$cmd'" }
            $sd = RegVal 'HKCU\Software\qlcconsole' 'StartMenu_Dir'
            if ($sd -and (Test-Path -LiteralPath (Join-Path $sd 'qlcconsole.lnk'))) {
                $t = $wsh.CreateShortcut((Join-Path $sd 'qlcconsole.lnk')).TargetPath
                if ($t -notlike "$InstA\*") { $why += "the Start Menu shortcut now starts '$t'" }
            }
            if ($why) { Fail 'win.takeover' ("installing to $InstB took over the install in $InstA without asking: " + ($why -join '; ')) }
            else { Pass 'win.takeover' 'the first install keeps its entry, associations and shortcuts' }
        }
        Uninstall-From $InstB | Out-Null
        Uninstall-From $InstA | Out-Null
    }
}
catch {
    Fail 'win.check-ran' "the check stopped: $($_.Exception.Message)"
    $Exit = 2
}
finally {
    # Whatever happened: our folders gone, the machine as it was.
    foreach ($d in @($InstA, $InstB)) {
        if (Test-Path -LiteralPath (Join-Path $d 'uninstall.exe')) { Uninstall-From $d | Out-Null }
        if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force -ErrorAction SilentlyContinue }
    }
    Get-Process -ErrorAction SilentlyContinue | Where-Object { try { $_.Path -like "$WorkDir\*" } catch { $false } } | Stop-Process -Force -ErrorAction SilentlyContinue
    Restore-State
    $rd = @(Restored-Differences)
    Check 'win.machine-restored' ($rd.Count -eq 0) "not as before the check: $($rd -join '; ')"
    $crashes = @()
    foreach ($d in $CrashDirs) {
        if (Test-Path $d) {
            Get-ChildItem -LiteralPath $d -ErrorAction SilentlyContinue |
                Where-Object { -not $CrashBefore.ContainsKey($_.FullName) -and $_.Name -match 'qlc' } |
                ForEach-Object { $crashes += $_.Name; Copy-Item -LiteralPath $_.FullName -Destination $Out -Recurse -ErrorAction SilentlyContinue; Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
        }
    }
    Check 'win.no-crash-dumps' ($crashes.Count -eq 0) "crashed: $($crashes -join ', ') (removed)"
    Remove-Item -LiteralPath $script:RunDir -Recurse -Force -ErrorAction SilentlyContinue
}

Emit "1..$($script:N)"
$script:Json | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $JsonFile -Encoding UTF8
Write-Host ""
Write-Host "==== qlcconsole release check: $SetupName on $env:COMPUTERNAME ===="
Write-Host ("{0} checks: {1} passed, {2} failed, {3} waived, {4} skipped" -f $script:N, ($script:N - $script:Fails.Count - $script:Waived.Count - $script:Skips), $script:Fails.Count, $script:Waived.Count, $script:Skips)
if ($script:Fails) { Write-Host "FAILED:"; $script:Fails | ForEach-Object { Write-Host "  $_" } }
if ($script:Waived) { Write-Host "Waived (known, not blocking):"; $script:Waived | ForEach-Object { Write-Host "  $_" } }
Write-Host "TAP: $TapFile"
Write-Host "JSON: $JsonFile"
if ($Exit -ne 0) { exit $Exit }
if ($script:Fails.Count -gt 0) { exit 1 }
exit 0
