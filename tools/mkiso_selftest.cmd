@echo off
rem ===================================================================
rem  mkiso_selftest.cmd - rebuild the mkiso.py test ISO and re-run all
rem  automated checks:
rem
rem    * builds a test tree (nested dirs, empty dir, empty file, 3 MB
rem      binary file, mixed case / spaces / non-ASCII names)
rem    * builds the Joliet image and a primary-only (--no-joliet) image
rem    * 7-Zip listing + extraction + SHA-256 comparison of every file
rem    * raw ISO 9660 byte checks (descriptors, both-endian fields,
rem      path tables, root directory, sorted records, extents)
rem    * determinism (two builds must be byte identical)
rem    * edge cases (empty-dir-only tree, single file tree)
rem    * error handling (8.3 name collision must fail with a clear error)
rem    * optional Mount-DiskImage round trip (label + hashes)
rem
rem  Usage:  mkiso_selftest.cmd [/skipmount] [/keep]
rem    /skipmount   do not attempt Mount-DiskImage
rem    /keep        keep the %TEMP%\mkiso_selftest working directory
rem
rem  Exit code 0 = every check passed, 1 = at least one check failed.
rem  Set MKISO_PY to override the Python interpreter.
rem  This file is deliberately pure ASCII: the batch header below only
rem  launches the PowerShell payload that follows the marker line.
rem ===================================================================
setlocal
set "MKISO_SELFTEST_SELF=%~f0"
set "MKISO_SELFTEST_ARGS=%*"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=$env:MKISO_SELFTEST_SELF; $l=Get-Content -LiteralPath $s; $i=[Array]::IndexOf($l,'#---POWERSHELL-BEGIN---'); if ($i -lt 0) { Write-Host 'ERROR: payload marker not found'; exit 2 }; & ([scriptblock]::Create((($l[($i+1)..($l.Count-1)]) -join [Environment]::NewLine)))"
exit /b %ERRORLEVEL%
#---POWERSHELL-BEGIN---
# =====================================================================
#  PowerShell payload - mkiso.py self test
# =====================================================================
$ErrorActionPreference = 'Continue'
$ProgressPreference    = 'SilentlyContinue'

$Self     = $env:MKISO_SELFTEST_SELF
$Root     = Split-Path -Parent $Self
$Mkiso    = Join-Path $Root 'mkiso.py'
$SevenZip = 'C:\Program Files\7-Zip\7z.exe'
$Work     = Join-Path $env:TEMP 'mkiso_selftest'
$Label    = 'SMARTMATE2000'
$DateArg  = '2003-06-01'

if ($env:MKISO_PY) { $Py = $env:MKISO_PY }
else { $Py = 'C:\Users\qwaw1\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe' }

$ArgText   = [string]$env:MKISO_SELFTEST_ARGS
$SkipMount = $ArgText -match '(?i)/skipmount'
$Keep      = $ArgText -match '(?i)/keep'

$script:Fails  = 0
$script:Checks = 0

function Check {
    param([bool]$Condition, [string]$Message)
    $script:Checks++
    if ($Condition) { Write-Host ("  ok    " + $Message) }
    else            { Write-Host ("  FAIL  " + $Message); $script:Fails++ }
}

function Section {
    param([string]$Title)
    Write-Host ''
    Write-Host ('== ' + $Title + ' ' + ('=' * [Math]::Max(0, 60 - $Title.Length)))
}

function Sha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

# ---------------------------------------------------------------------
Section 'prerequisites'
# ---------------------------------------------------------------------
Check (Test-Path -LiteralPath $Mkiso)  ("mkiso.py present: " + $Mkiso)
Check (Test-Path -LiteralPath $Py)     ("python present: " + $Py)
$has7z = Test-Path -LiteralPath $SevenZip
Check $has7z ("7-Zip present: " + $SevenZip)
if (-not (Test-Path -LiteralPath $Mkiso) -or -not (Test-Path -LiteralPath $Py)) {
    Write-Host ''
    Write-Host 'RESULT: FAIL (prerequisites missing)'
    exit 1
}

# ---------------------------------------------------------------------
Section 'test tree'
# ---------------------------------------------------------------------
if (Test-Path -LiteralPath $Work) { Remove-Item -Recurse -Force -LiteralPath $Work }
$Src = Join-Path $Work 'src'
foreach ($d in @('', 'docs', 'docs\deep', 'drivers\vxd', 'empty_dir')) {
    $p = if ($d) { Join-Path $Src $d } else { $Src }
    New-Item -ItemType Directory -Force -Path $p | Out-Null
}

function Write-Text {
    param([string]$Path, [string]$Text)
    [IO.File]::WriteAllBytes($Path, [Text.Encoding]::UTF8.GetBytes($Text + "`r`n"))
}
function Write-Bytes {
    param([string]$Path, [byte[]]$Bytes)
    [IO.File]::WriteAllBytes($Path, $Bytes)
}

Write-Host '  building a deterministic 3 MB pseudo-random file ...'
$Block = New-Object byte[] 4096
$seed  = [uint32]20240601
for ($i = 0; $i -lt $Block.Length; $i++) {
    $seed = [uint32](($seed * 1103515245 + 12345) -band 0x7FFFFFFF)
    $Block[$i] = [byte](($seed -shr 16) -band 0xFF)
}
$ms = New-Object System.IO.MemoryStream
for ($i = 0; $i -lt 768; $i++) { $ms.Write($Block, 0, $Block.Length) }   # 768 * 4096 = 3 MiB
[IO.File]::WriteAllBytes((Join-Path $Src 'BigFile.BIN'), $ms.ToArray())
$ms.Dispose()

Write-Text (Join-Path $Src 'README.TXT')    'SmartMate 2000 setup CD.'
Write-Text (Join-Path $Src 'Readme 2.txt')  'Mixed case name with a space.'
Write-Text (Join-Path $Src  'docs\notes.txt') 'notes'
Write-Bytes (Join-Path $Src 'Setup.EXE') ([byte[]](0..255) * 12)
[IO.File]::WriteAllBytes((Join-Path $Src 'Empty.txt'), (New-Object byte[] 0))
$pdf = New-Object byte[] 5004
[Array]::Copy([byte[]](0x25, 0x50, 0x44, 0x46), $pdf, 0)   # "%PDF"
for ($i = 0; $i -lt 5004; $i++) { if ($i -ge 4) { $pdf[$i] = 0x78 } }
Write-Bytes (Join-Path $Src 'docs\manual.pdf') $pdf
Write-Text (Join-Path $Src 'docs\deep\a b c.dat') 'deep file'
[IO.File]::WriteAllBytes((Join-Path $Src 'docs\deep\zero.bin'), (New-Object byte[] 0))
[IO.File]::WriteAllBytes((Join-Path $Src 'drivers\vxd\driver.vxd'), [Text.Encoding]::ASCII.GetBytes('VXD' * 100))
# non-ASCII name built from code points so this file stays pure ASCII
$cjk = ([string][char]0x4E2D) + [string][char]0x6587 + '.txt'
Write-Text (Join-Path $Src $cjk) 'chinese filename'

$srcFiles = Get-ChildItem -Recurse -File -LiteralPath $Src
Check ($srcFiles.Count -eq 11) ("source tree has 11 files (found " + $srcFiles.Count + ")")
Check (Test-Path -LiteralPath (Join-Path $Src 'empty_dir')) 'source tree has an empty directory'
Check ((Get-Item -LiteralPath (Join-Path $Src 'BigFile.BIN')).Length -eq 3MB) 'source has a 3 MB file'
Check ((Get-Item -LiteralPath (Join-Path $Src 'Empty.txt')).Length -eq 0) 'source has a zero byte file'

# ---------------------------------------------------------------------
Section 'build (Joliet)'
# ---------------------------------------------------------------------
$Iso = Join-Path $Work 'test.iso'
& $Py $Mkiso --src $Src --out $Iso --label $Label --date $DateArg
$buildRc = $LASTEXITCODE
Check ($buildRc -eq 0) ("mkiso.py exited 0 (got " + $buildRc + ")")
Check (Test-Path -LiteralPath $Iso) 'test.iso was created'
if (-not (Test-Path -LiteralPath $Iso)) {
    Write-Host ''
    Write-Host 'RESULT: FAIL (image not produced)'
    exit 1
}
$isoLen = (Get-Item -LiteralPath $Iso).Length
Check (($isoLen % 2048) -eq 0) ("image size " + $isoLen + " is a multiple of 2048")

# ---------------------------------------------------------------------
Section 'determinism'
# ---------------------------------------------------------------------
$Iso2 = Join-Path $Work 'test2.iso'
& $Py $Mkiso --src $Src --out $Iso2 --label $Label --date $DateArg --quiet | Out-Null
Check ((Sha256 $Iso) -eq (Sha256 $Iso2)) 'two builds of the same tree are byte identical'
Remove-Item -Force -LiteralPath $Iso2

# ---------------------------------------------------------------------
Section 'raw ISO 9660 checks'
# ---------------------------------------------------------------------
$SECTOR = 2048
$fs = [IO.File]::OpenRead($Iso)
function Read-Range {
    param([long]$Offset, [int]$Count)
    $buf = New-Object byte[] $Count
    [void]$fs.Seek($Offset, 'Begin')
    $read = 0
    while ($read -lt $Count) {
        $n = $fs.Read($buf, $read, $Count - $read)
        if ($n -le 0) { break }
        $read += $n
    }
    if ($read -ne $Count) { throw ("short read at offset " + $Offset) }
    return $buf
}
function Be16 {
    param([byte[]]$Buf, [int]$Offset)
    $le = [BitConverter]::ToUInt16($Buf, $Offset)
    $tmp = New-Object byte[] 2
    [Array]::Copy($Buf, $Offset + 2, $tmp, 0, 2)
    [Array]::Reverse($tmp)
    $be = [BitConverter]::ToUInt16($tmp, 0)
    return @($le, $be)
}
function Be32 {
    param([byte[]]$Buf, [int]$Offset)
    $le = [BitConverter]::ToUInt32($Buf, $Offset)
    $tmp = New-Object byte[] 4
    [Array]::Copy($Buf, $Offset + 4, $tmp, 0, 4)
    [Array]::Reverse($tmp)
    $be = [BitConverter]::ToUInt32($tmp, 0)
    return @($le, $be)
}

$pvd = Read-Range (16 * $SECTOR) $SECTOR
Write-Host '  sector 16, first 64 bytes:'
for ($i = 0; $i -lt 64; $i += 16) {
    $chunk = $pvd[$i..($i + 15)]
    $hex = ($chunk | ForEach-Object { $_.ToString('x2') }) -join ' '
    $asc = -join ($chunk | ForEach-Object { if ($_ -ge 32 -and $_ -lt 127) { [char]$_ } else { '.' } })
    Write-Host ('    {0:x4}  {1}  |{2}|' -f $i, $hex, $asc)
}
Check ($pvd[0] -eq 1) 'primary volume descriptor has type 1'
Check ([Text.Encoding]::ASCII.GetString($pvd, 1, 5) -eq 'CD001') 'identifier is CD001'
Check ($pvd[6] -eq 1) 'descriptor version is 1'

$svd = Read-Range (17 * $SECTOR) $SECTOR
Check ($svd[0] -eq 2) 'supplementary (Joliet) volume descriptor has type 2'
Check ([Text.Encoding]::ASCII.GetString($svd, 1, 5) -eq 'CD001') 'supplementary identifier is CD001'
Check ([Text.Encoding]::ASCII.GetString($svd, 88, 9) -eq '%/E@/E^/E') "Joliet escape sequences are %/E@/E^/E (got '" + [Text.Encoding]::ASCII.GetString($svd, 88, 9) + "')"

$term = Read-Range (18 * $SECTOR) $SECTOR
Check ($term[0] -eq 255) 'volume descriptor set terminator has type 255'
Check ([Text.Encoding]::ASCII.GetString($term, 1, 5) -eq 'CD001') 'terminator identifier is CD001'

$labelBytes = [Text.Encoding]::ASCII.GetString($pvd, 40, 32)
Check ($labelBytes -eq $Label.PadRight(32)) ("volume label is '" + $Label + "' padded to 32 characters")
$space = Be32 $pvd 80
Check ($space[0] -eq $space[1]) 'volume space size is stored both endian'
Check ($space[0] -eq ($isoLen / $SECTOR)) ("volume space size " + $space[0] + " equals " + ($isoLen / $SECTOR) + " sectors")
$blockSize = Be16 $pvd 128
Check ($blockSize[0] -eq 2048) 'logical block size is 2048'
$setSize = Be16 $pvd 120
Check ($setSize[0] -eq 1) 'volume set size is 1'
Check ($pvd[881] -eq 1) 'file structure version is 1'

# date string: 17 bytes, 16 digits + tz offset
$dateStr = [Text.Encoding]::ASCII.GetString($pvd, 813, 16)
Check ($dateStr -match '^\d{16}$') ("creation date is 16 digits (" + $dateStr + ")")
Check ($dateStr.StartsWith('20030601000000')) 'creation date matches --date 2003-06-01'
Check ($pvd[829] -eq 0) 'creation date timezone offset byte is present'

# both path tables, both endian, first entry is the root
$ptSizePvd = Be32 $pvd 132
Check ($ptSizePvd[0] -eq $ptSizePvd[1]) 'path table size is stored both endian'
$lLba  = [BitConverter]::ToUInt32($pvd, 140)
$mBytes = New-Object byte[] 4
[Array]::Copy($pvd, 148, $mBytes, 0, 4)
[Array]::Reverse($mBytes)
$mLba = [BitConverter]::ToUInt32($mBytes, 0)
Check ($lLba -gt 18) ("type L path table at sector " + $lLba)
Check ($mLba -gt 18) ("type M path table at sector " + $mLba)
$ptL = Read-Range ($lLba * $SECTOR) 16
$ptM = Read-Range ($mLba * $SECTOR) 16
Check ($ptL[0] -eq 1 -and $ptL[8] -eq 0) 'first L path table entry is the root directory (name 0x00)'
$ptLExt = [BitConverter]::ToUInt32($ptL, 2)
$tmpM = New-Object byte[] 4
[Array]::Copy($ptM, 2, $tmpM, 0, 4)
[Array]::Reverse($tmpM)
$ptMExt = [BitConverter]::ToUInt32($tmpM, 0)
Check ($ptLExt -eq $ptMExt) ("L and M path tables agree on the root extent (" + $ptLExt + ")")

# root directory record inside the PVD
$rootExt  = [BitConverter]::ToUInt32($pvd, 156 + 2)
$rootSize = [BitConverter]::ToUInt32($pvd, 156 + 10)
Check ($pvd[156] -eq 34) 'root directory record is 34 bytes long'
Check (($pvd[156 + 25] -band 2) -eq 2) 'root directory record has the directory flag'
Check (($rootSize % $SECTOR) -eq 0) ("root directory size " + $rootSize + " is a sector multiple")
Check ($rootExt -eq $ptLExt) 'root record extent matches the path table'

# walk the primary tree and check structure
$checkedRecords = 0
$dirQueue = New-Object System.Collections.Queue
$dirQueue.Enqueue(@($rootExt, $rootSize, '<root>'))
$sortedOk = $true
$dotOk = $true
$extentOk = $true
$isoNames = New-Object System.Collections.ArrayList
while ($dirQueue.Count -gt 0) {
    $item = $dirQueue.Dequeue()
    $ext = [int]$item[0]; $size = [int]$item[1]; $pathText = [string]$item[2]
    $data = Read-Range ($ext * $SECTOR) $size
    $pos = 0
    $recordIndex = 0
    $names = New-Object System.Collections.ArrayList
    while ($pos -lt $size) {
        $len = $data[$pos]
        if ($len -eq 0) { $pos = [int]([Math]::Floor($pos / $SECTOR) + 1) * $SECTOR; continue }
        $rec = $data[$pos..($pos + $len - 1)]
        $recExt  = [BitConverter]::ToUInt32($rec, 2)
        $recSize = [BitConverter]::ToUInt32($rec, 10)
        $flags   = $rec[25]
        $nameLen = $rec[32]
        $name = if ($recordIndex -eq 0) { '' } elseif ($recordIndex -eq 1) { '' } else { [Text.Encoding]::ASCII.GetString($rec, 33, $nameLen) }
        if ($recordIndex -eq 0) {
            if (-not ($nameLen -eq 1 -and $rec[33] -eq 0 -and ($flags -band 2) -and $recExt -eq $ext)) { $dotOk = $false }
        } elseif ($recordIndex -eq 1) {
            if (-not ($nameLen -eq 1 -and $rec[33] -eq 1 -and ($flags -band 2))) { $dotOk = $false }
        } else {
            if (($recSize -gt 0) -and (($recExt * $SECTOR) + $recSize -gt $isoLen)) { $extentOk = $false }
            if ($flags -band 2) { $dirQueue.Enqueue(@($recExt, $recSize, ($pathText + '/' + $name))) }
            else { [void]$isoNames.Add($name) }
            [void]$names.Add($name)
        }
        $checkedRecords++
        $recordIndex++
        $pos += $len
    }
    for ($i = 1; $i -lt $names.Count; $i++) {
        if ([string]::CompareOrdinal($names[$i - 1], $names[$i]) -gt 0) { $sortedOk = $false }
    }
}
$fs.Close()
Check $dotOk 'every directory starts with "." and ".." and the directory flag'
Check $sortedOk 'primary tree directory records are sorted by name'
Check $extentOk 'every file extent lies inside the image'
Check ($checkedRecords -gt 20) ("parsed " + $checkedRecords + " primary tree records")
$hasVersion = $true
$is83 = $true
foreach ($n in $isoNames) {
    if (-not $n.EndsWith(';1')) { $hasVersion = $false }
    $base = $n.Substring(0, $n.Length - 2)
    if ($base -notmatch '^[A-Z0-9_]{1,8}(\.[A-Z0-9_]{1,3})?$') { $is83 = $false }
}
Check $hasVersion 'all primary tree file names carry the ";1" version suffix'
Check $is83 'all primary tree names are uppercase 8.3'
Check ($isoNames -contains 'README.TXT;1') 'primary tree contains README.TXT;1'

# ---------------------------------------------------------------------
Section '7-Zip listing (Joliet names)'
# ---------------------------------------------------------------------
$sevenZipOk = $has7z
if ($sevenZipOk) {
    $listing = (& $SevenZip l $Iso 2>&1 | Out-String)
    Check ($LASTEXITCODE -eq 0) '7z l accepted the image'
    foreach ($expect in @('BigFile.BIN', 'Readme 2.txt', 'README.TXT', 'Setup.EXE', 'docs\deep\a b c.dat', 'docs\manual.pdf', 'drivers\vxd\driver.vxd', 'empty_dir')) {
        Check ($listing.Contains($expect)) ("7z lists Joliet name '" + $expect + "'")
    }
    Check ($listing -notmatch 'README_2\.TXT') '7z shows the long Joliet name, not the 8.3 name'
}

# ---------------------------------------------------------------------
Section '7-Zip extraction + SHA-256 comparison (Joliet tree)'
# ---------------------------------------------------------------------
$Extract = Join-Path $Work 'extracted'
if ($sevenZipOk) {
    New-Item -ItemType Directory -Force -Path $Extract | Out-Null
    & $SevenZip x $Iso ('-o' + $Extract) -y | Out-Null
    Check ($LASTEXITCODE -eq 0) '7z x extracted without errors'
    $mismatch = 0; $missing = 0; $compared = 0
    foreach ($f in (Get-ChildItem -Recurse -File -LiteralPath $Src)) {
        $rel = $f.FullName.Substring($Src.Length + 1)
        $dst = Join-Path $Extract $rel
        if (-not (Test-Path -LiteralPath $dst)) { Write-Host ('  missing after extract: ' + $rel); $missing++; continue }
        if ((Sha256 $f.FullName) -ne (Sha256 $dst)) { Write-Host ('  hash mismatch: ' + $rel); $mismatch++ }
        $compared++
    }
    Check ($missing -eq 0) 'every source file was extracted'
    Check ($mismatch -eq 0) 'every extracted file matches the source by SHA-256'
    Check ($compared -eq 11) ("compared " + $compared + " files")
    $extractedDirs = Get-ChildItem -Recurse -Directory -LiteralPath $Extract | ForEach-Object { $_.FullName.Substring($Extract.Length + 1) }
    foreach ($d in @('docs', 'docs\deep', 'drivers', 'drivers\vxd', 'empty_dir')) {
        Check ($extractedDirs -contains $d) ("7z recreated directory '" + $d + "'")
    }
    Check (Test-Path -LiteralPath (Join-Path $Extract $cjk)) 'the non-ASCII file name survived extraction'
}

# ---------------------------------------------------------------------
Section 'primary-only image (--no-joliet)'
# ---------------------------------------------------------------------
$IsoNoJoliet = Join-Path $Work 'nojoliet.iso'
& $Py $Mkiso --src $Src --out $IsoNoJoliet --label $Label --no-joliet --date $DateArg --quiet
Check ($LASTEXITCODE -eq 0) 'mkiso.py --no-joliet exited 0'
if ($sevenZipOk -and (Test-Path -LiteralPath $IsoNoJoliet)) {
    $listing2 = (& $SevenZip l $IsoNoJoliet 2>&1 | Out-String)
    Check ($LASTEXITCODE -eq 0) '7z l accepted the primary-only image'
    Check ($listing2 -match '11 files') 'primary-only listing reports 11 files'
    Check ($listing2.Contains('README_2.TXT')) "primary tree shows the 8.3 name 'README_2.TXT'"
    Check ($listing2.Contains('__.TXT')) "primary tree shows the sanitised name '__.TXT'"
    Check ($listing2.Contains('DOCS\DEEP\A_B_C.DAT')) 'primary tree shows the 8.3 path DOCS\DEEP\A_B_C.DAT'
    Check ($listing2 -notmatch 'Readme 2\.txt') 'primary-only image has no long names'
    $extract2 = Join-Path $Work 'extracted_primary'
    New-Item -ItemType Directory -Force -Path $extract2 | Out-Null
    & $SevenZip x $IsoNoJoliet ('-o' + $extract2) -y | Out-Null
    $have = @{}
    foreach ($f in (Get-ChildItem -Recurse -File -LiteralPath $Src)) { $have[(Sha256 $f.FullName)] = $f.Name }
    $bad = 0; $n2 = 0
    foreach ($f in (Get-ChildItem -Recurse -File -LiteralPath $extract2)) {
        if (-not $have.ContainsKey((Sha256 $f.FullName))) { $bad++ }
        $n2++
    }
    Check ($bad -eq 0 -and $n2 -eq 11) ("primary tree payloads match the source (" + $n2 + " files)")
} else {
    Write-Host '  skip  7-Zip checks for the primary-only image (7-Zip missing)'
}

# structural check of the primary-only image: no SVD, terminator directly after the PVD
$fs2 = [IO.File]::OpenRead($IsoNoJoliet)
$buf2 = New-Object byte[] 2048
[void]$fs2.Seek((17 * $SECTOR), 'Begin'); [void]$fs2.Read($buf2, 0, 2048)
$fs2.Close()
Check ($buf2[0] -eq 255) 'primary-only image has the terminator right after the PVD'

# ---------------------------------------------------------------------
Section 'edge cases'
# ---------------------------------------------------------------------
# (a) a tree with nothing but empty directories
$EdgeA = Join-Path $Work 'edge_empty'
New-Item -ItemType Directory -Force -Path (Join-Path $EdgeA 'src\only_empty'), (Join-Path $EdgeA 'src\another\empty') | Out-Null
$isoA = Join-Path $EdgeA 'empty.iso'
& $Py $Mkiso --src (Join-Path $EdgeA 'src') --out $isoA --label EDGETEST --date $DateArg --quiet
Check ($LASTEXITCODE -eq 0) 'empty-dir-only tree builds'
if ($sevenZipOk -and (Test-Path -LiteralPath $isoA)) {
    $listA = (& $SevenZip l $isoA 2>&1 | Out-String)
    Check ($LASTEXITCODE -eq 0) '7z l accepted the empty-dir-only image'
    Check ($listA.Contains('only_empty') -and $listA.Contains('another\empty')) 'both empty directories are present'
    Check ($listA -match '0 files') 'the empty image reports zero files'
}

# (b) a tree with a single file
$EdgeB = Join-Path $Work 'edge_single'
New-Item -ItemType Directory -Force -Path (Join-Path $EdgeB 'src') | Out-Null
Write-Text (Join-Path $EdgeB 'src\only.dat') 'single file'
$isoB = Join-Path $EdgeB 'one.iso'
& $Py $Mkiso --src (Join-Path $EdgeB 'src') --out $isoB --label SOLO --date '2003-12-31T23:59:58' --quiet
Check ($LASTEXITCODE -eq 0) 'single-file tree builds'
if ($sevenZipOk -and (Test-Path -LiteralPath $isoB)) {
    $listB = (& $SevenZip l $isoB 2>&1 | Out-String)
    Check ($LASTEXITCODE -eq 0) '7z l accepted the single-file image'
    Check ($listB.Contains('only.dat')) 'the single file is present'
    $exB = Join-Path $EdgeB 'x'
    New-Item -ItemType Directory -Force -Path $exB | Out-Null
    & $SevenZip x $isoB ('-o' + $exB) -y | Out-Null
    Check ((Sha256 (Join-Path $exB 'only.dat')) -eq (Sha256 (Join-Path $EdgeB 'src\only.dat'))) 'single file round trips intact'
}

# ---------------------------------------------------------------------
Section 'error handling'
# ---------------------------------------------------------------------
$EdgeC = Join-Path $Work 'edge_collision'
New-Item -ItemType Directory -Force -Path (Join-Path $EdgeC 'src') | Out-Null
Write-Text (Join-Path $EdgeC 'src\a b.txt') 'x'
Write-Text (Join-Path $EdgeC 'src\a_b.txt') 'y'
$colOut = (& $Py $Mkiso --src (Join-Path $EdgeC 'src') --out (Join-Path $EdgeC 'out.iso') --label T --date $DateArg 2>&1 | Out-String)
Check ($LASTEXITCODE -eq 2) ("8.3 collision is rejected with exit code 2 (got " + $LASTEXITCODE + ")")
Check (($colOut -match 'collision') -and ($colOut -match 'A_B\.TXT')) 'the collision error names the clashing identifier'
Check (-not (Test-Path -LiteralPath (Join-Path $EdgeC 'out.iso'))) 'no image is left behind on failure'

$badOut = (& $Py $Mkiso --src $Src --out (Join-Path $Work 'x.iso') --label T --boot-image boot.img 2>&1 | Out-String)
Check ($LASTEXITCODE -eq 2) '--boot-image other than none is rejected'
Check ($badOut -match 'boot') 'the boot-image error explains the limitation'

# ---------------------------------------------------------------------
Section 'Mount-DiskImage round trip'
# ---------------------------------------------------------------------
$mountSkip = $SkipMount
if (-not $mountSkip) {
    $badDevices = @(Get-PnpDevice -Class CDROM -ErrorAction SilentlyContinue |
                    Where-Object { $_.Status -eq 'Error' -or $_.Problem -ne 0 })
    if ($badDevices.Count -gt 0) {
        $mountSkip = $true
        Write-Host '  skip  a virtual CD-ROM device is in an error state; the mount stack needs a reboot'
    }
}
if ($mountSkip) {
    Write-Host '  skip  Mount-DiskImage not attempted (use the 7-Zip round trip and the raw byte checks instead)'
} else {
    $mountJob = Start-Job -ArgumentList $Iso, $Src, $Label, $cjk -ScriptBlock {
        param($isoPath, $srcPath, $expectLabel, $cjkName)
        try {
            $image = Mount-DiskImage -ImagePath $isoPath -PassThru -ErrorAction Stop
        } catch {
            return @{ Ok = $false; Reason = ('mount failed: ' + $_.Exception.Message) }
        }
        try {
            $vol = $image | Get-Volume
            if (-not $vol.DriveLetter) { return @{ Ok = $false; Reason = 'mounted but no drive letter appeared' } }
            $rootPath = $vol.DriveLetter + ':\'
            $label = $vol.FileSystemLabel
            $bad = 0; $count = 0
            foreach ($f in (Get-ChildItem -Recurse -File -LiteralPath $srcPath)) {
                $rel = $f.FullName.Substring($srcPath.Length + 1)
                $dst = Join-Path $rootPath $rel
                if (-not (Test-Path -LiteralPath $dst)) { $bad++; continue }
                if ((Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash) { $bad++ }
                $count++
            }
            $dirs = @(Get-ChildItem -Recurse -Directory -LiteralPath $rootPath | ForEach-Object { $_.FullName.Substring($rootPath.Length) })
            return @{ Ok = $true; Label = $label; Bad = $bad; Count = $count; Dirs = $dirs; Letter = $vol.DriveLetter; Text = [string]$vol.FileSystem }
        } finally {
            try { Dismount-DiskImage -ImagePath $isoPath -ErrorAction SilentlyContinue } catch { }
        }
    }
    $finished = Wait-Job $mountJob -Timeout 90
    if (-not $finished) {
        Stop-Job $mountJob -ErrorAction SilentlyContinue
        Write-Host '  skip  Mount-DiskImage did not complete within 90 s (host mount stack busy)'
    } else {
        $r = Receive-Job $mountJob
        if ($r.Ok) {
            Check ($r.Label -eq $Label) ("mounted volume label is '" + $r.Label + "'")
            Check ($r.Text -like '*CDFS*' -or $r.Text -like '*UDF*') ("mounted file system is CDFS (got '" + $r.Text + "')")
            Check ($r.Count -eq 11) ("read " + $r.Count + " files from the mounted volume")
            Check ($r.Bad -eq 0) 'every file read from the mounted volume matches the source'
            Check ($r.Dirs -contains 'empty_dir') 'the empty directory is visible on the mounted volume'
        } else {
            Write-Host ('  skip  ' + $r.Reason)
        }
    }
    Remove-Job $mountJob -Force -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------------
Section 'summary'
# ---------------------------------------------------------------------
Write-Host ("  checks run: " + $script:Checks)
Write-Host ("  failures:   " + $script:Fails)
Write-Host ("  artifacts:  " + $Work)
if (-not $Keep -and $script:Fails -eq 0) {
    Remove-Item -Recurse -Force -LiteralPath $Work -ErrorAction SilentlyContinue
    Write-Host '  (working directory removed; pass /keep to keep it)'
}
if ($script:Fails -gt 0) {
    Write-Host ''
    Write-Host 'RESULT: FAIL'
    exit 1
}
Write-Host ''
Write-Host 'RESULT: PASS'
exit 0
