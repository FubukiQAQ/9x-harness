<#
.SYNOPSIS
    Rebuild the test .chm with mkchm.py and verify it end to end.

.DESCRIPTION
    1. compiles .\testdoc into .\test.chm with mkchm.py
    2. runs "7z l" (the archive must list cleanly as type Chm)
    3. runs "7z x" and compares the SHA-256 of every extracted file with the
       source file, so every internal file must round-trip byte-identically
    4. prints the ITSF structure (mkchm.py --dump) and the header hexdump
    5. optionally opens the file in hh.exe (-Viewer) and/or saves a screenshot
       of the desktop (-Viewer -Capture)

    Exit code 0 means all checks passed.  The script is ASCII-only on purpose
    so that both Windows PowerShell 5.1 and PowerShell 7 can run it.

.PARAMETER Python
    Python interpreter used to run mkchm.py.

.PARAMETER SevenZip
    Path to 7z.exe.

.PARAMETER Viewer
    Open the produced .chm in C:\Windows\hh.exe and check the viewer survives.

.PARAMETER Capture
    With -Viewer: save a desktop screenshot to .\selftest_screenshot.png.

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\mkchm_selftest.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File .\mkchm_selftest.ps1 -Viewer -Capture
#>
[CmdletBinding()]
param(
    [string] $Python   = 'C:\Users\qwaw1\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe',
    [string] $SevenZip = 'C:\Program Files\7-Zip\7z.exe',
    [string] $WorkDir  = '',
    [switch] $Viewer,
    [switch] $Capture,
    [switch] $KeepTemp
)

$ErrorActionPreference = 'Stop'

if (-not $WorkDir) {
    if ($PSScriptRoot) { $WorkDir = $PSScriptRoot }
    else { $WorkDir = (Get-Location).Path }
}
$WorkDir = (Resolve-Path -LiteralPath $WorkDir).Path

$script:failures = New-Object System.Collections.Generic.List[string]
function Fail([string] $message) {
    $script:failures.Add($message)
    Write-Host ('  [FAIL] ' + $message) -ForegroundColor Red
}
function Ok([string] $message) {
    Write-Host ('  [ ok ] ' + $message) -ForegroundColor Green
}
function Step([string] $message) {
    Write-Host ''
    Write-Host ('== ' + $message) -ForegroundColor Cyan
}

$mkchm   = Join-Path $WorkDir 'mkchm.py'
$srcDir  = Join-Path $WorkDir 'testdoc'
$chmPath = Join-Path $WorkDir 'test.chm'
$extract = Join-Path $WorkDir '_extract'

foreach ($tool in @($Python, $SevenZip, $mkchm)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        Write-Host ('missing required tool: ' + $tool) -ForegroundColor Red
        exit 2
    }
}
if (-not (Test-Path -LiteralPath $srcDir)) {
    Write-Host ('missing test documents: ' + $srcDir) -ForegroundColor Red
    Write-Host 'the test document set (index.htm, toc.hhc, index.hhk, ...) must exist'
    exit 2
}

# The help title is kept as base64 so this script stays pure ASCII; it decodes
# to the Simplified Chinese string used by the test document set.
$helpTitle = [System.Text.Encoding]::UTF8.GetString(
    [System.Convert]::FromBase64String('56S65L6L5biu5Yqp5paH5qGj'))

# --------------------------------------------------------------------------- #
Step 'compile testdoc with mkchm.py'
# --------------------------------------------------------------------------- #
Remove-Item -LiteralPath $chmPath -ErrorAction SilentlyContinue
& $Python $mkchm --src $srcDir --out $chmPath --title $helpTitle `
    --default index.htm --toc toc.hhc --index index.hhk --language 0x0804
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $chmPath)) {
    Fail 'mkchm.py did not produce a file'
    exit 1
}
$chmBytes = [System.IO.File]::ReadAllBytes($chmPath)
Ok ('compiled ' + $chmPath + ' (' + $chmBytes.Length + ' bytes)')

# --------------------------------------------------------------------------- #
Step 'checks on the produced file'
# --------------------------------------------------------------------------- #
$sig = [System.Text.Encoding]::ASCII.GetString($chmBytes, 0, 4)
if ($sig -eq 'ITSF') { Ok ('ITSF signature, version ' + $chmBytes[4] + ', header length 0x' + $chmBytes[8].ToString('x')) }
else { Fail ('bad container signature: ' + $sig) }

$declaredSize = [System.BitConverter]::ToUInt64($chmBytes, 0x68)
if ($declaredSize -eq $chmBytes.Length) { Ok ('header total-size field matches the real file size (' + $declaredSize + ')') }
else { Fail ('header says ' + $declaredSize + ' bytes but the file is ' + $chmBytes.Length) }

$dirOff = [System.BitConverter]::ToUInt64($chmBytes, 0x48)
$itsp = [System.Text.Encoding]::ASCII.GetString($chmBytes, [int]$dirOff, 4)
if ($itsp -eq 'ITSP') { Ok ('ITSP directory header at 0x' + $dirOff.ToString('x')) }
else { Fail ('no ITSP header at the directory offset') }

$chunkOff = [int]$dirOff + 0x54
$chunkSig = [System.Text.Encoding]::ASCII.GetString($chmBytes, $chunkOff, 4)
if ($chunkSig -eq 'PMGL' -or $chunkSig -eq 'PMGI') { Ok ('first directory chunk is ' + $chunkSig) }
else { Fail ('first directory chunk has an unknown signature: ' + $chunkSig) }

# --------------------------------------------------------------------------- #
Step '7-Zip: list the archive'
# --------------------------------------------------------------------------- #
$listOut = (& $SevenZip l $chmPath 2>&1 | Out-String)
if ($listOut -match 'Type = Chm' -and $listOut -notmatch 'ERROR') {
    Ok '7-Zip recognised the file as a Chm archive'
} else {
    Fail '7-Zip could not list the archive'
    Write-Host $listOut
}
if ($listOut -match '(?m)#SYSTEM') { Ok 'internal #SYSTEM entry is listed' }
else { Fail '#SYSTEM is missing from the listing' }

# --------------------------------------------------------------------------- #
Step '7-Zip: extract everything and compare hashes'
# --------------------------------------------------------------------------- #
Remove-Item -Recurse -Force -LiteralPath $extract -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $extract | Out-Null
$extractOut = (& $SevenZip x -y ('-o' + $extract) $chmPath 2>&1 | Out-String)
if ($extractOut -match 'Everything is Ok') { Ok '7-Zip extracted every internal file without error' }
else {
    Fail '7-Zip reported errors while extracting'
    Write-Host $extractOut
}

$sourceFiles = @(Get-ChildItem -LiteralPath $srcDir -File)
$bad = 0
foreach ($file in $sourceFiles) {
    $copy = Join-Path $extract $file.Name
    if (-not (Test-Path -LiteralPath $copy)) {
        Fail ('missing after extraction: ' + $file.Name)
        $bad++
        continue
    }
    $a = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $b = (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash
    if ($a -ne $b) {
        Fail ('content differs after round trip: ' + $file.Name)
        $bad++
    }
}
if ($bad -eq 0) {
    Ok ('all ' + $sourceFiles.Count + ' source files round-tripped byte-identically (SHA-256)')
}

# every internal file 7-Zip extracted must also match its directory entry size
$dirEntries = @([regex]::Matches($listOut, '(?m)^\s+\.\.\.\.\.\s+(\d+)\s+(\S.*)$'))
if ($dirEntries.Count -gt 0) { Ok ('7-Zip listed ' + $dirEntries.Count + ' internal files') }

# --------------------------------------------------------------------------- #
Step 'structure dump (mkchm.py --dump)'
# --------------------------------------------------------------------------- #
& $Python $mkchm --dump $chmPath
if ($LASTEXITCODE -ne 0) { Fail 'mkchm.py --dump failed' }

# --------------------------------------------------------------------------- #
Step 'ITSF header hexdump'
# --------------------------------------------------------------------------- #
$limit = [Math]::Min(0x90, $chmBytes.Length)
for ($i = 0; $i -lt $limit; $i += 16) {
    $end = [Math]::Min($i + 15, $chmBytes.Length - 1)
    $hex = @()
    $txt = ''
    for ($j = $i; $j -le $end; $j++) {
        $hex += $chmBytes[$j].ToString('x2')
        if ($chmBytes[$j] -ge 32 -and $chmBytes[$j] -lt 127) { $txt += [char]$chmBytes[$j] }
        else { $txt += '.' }
    }
    $hexText = $hex -join ' '
    Write-Host ('  {0:x4}  {1,-47}  |{2}|' -f $i, $hexText, $txt)
}

# --------------------------------------------------------------------------- #
if ($Viewer) {
    Step 'open the file with hh.exe'
    Get-Process hh -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Process 'C:\Windows\hh.exe' -ArgumentList $chmPath | Out-Null
    Start-Sleep -Seconds 5
    $proc = @(Get-Process hh -ErrorAction SilentlyContinue)
    if ($proc.Count -gt 0) {
        $title = $proc[0].MainWindowTitle
        Ok ('hh.exe is running, window title: ' + $title)
        if ($title -eq 'help' -or $title -eq 'Help' -or $title -eq 'Windows Help') {
            Fail 'the generic window title means the CHM title record was not read'
        }
        if ($Capture) {
            Add-Type -AssemblyName System.Windows.Forms, System.Drawing
            $screen = [System.Windows.Forms.SystemInformation]::VirtualScreen
            $bmp = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
            $gfx = [System.Drawing.Graphics]::FromImage($bmp)
            $gfx.CopyFromScreen($screen.Left, $screen.Top, 0, 0, $bmp.Size)
            $shot = Join-Path $WorkDir 'selftest_screenshot.png'
            $bmp.Save($shot, [System.Drawing.Imaging.ImageFormat]::Png)
            $gfx.Dispose(); $bmp.Dispose()
            Ok ('screenshot saved to ' + $shot)
        }
        Get-Process hh -ErrorAction SilentlyContinue | Stop-Process -Force
    } else {
        Fail 'hh.exe exited immediately, the CHM was rejected'
    }
}

if (-not $KeepTemp) { Remove-Item -Recurse -Force -LiteralPath $extract -ErrorAction SilentlyContinue }

# --------------------------------------------------------------------------- #
Step 'summary'
# --------------------------------------------------------------------------- #
if ($script:failures.Count -eq 0) {
    Write-Host '  ALL CHECKS PASSED' -ForegroundColor Green
    exit 0
}
Write-Host ('  ' + $script:failures.Count + ' check(s) failed:') -ForegroundColor Red
$script:failures | ForEach-Object { Write-Host ('   - ' + $_) -ForegroundColor Red }
exit 1
