# =====================================================================
#  SmartMate 2000 - 构建脚本
#  用法:  pwsh -File build.ps1 [-Target all] [-Clean]
#  需要:  vendor\mingw32 (i686 msvcrt 工具链)、vendor\bearssl-0.6
# =====================================================================
param(
    [string]$Target = 'all',
    [string]$Brand  = 'brand.ini',
    [switch]$Clean,
    [switch]$Verbose
)

$ErrorActionPreference = 'Continue'
$Root = $PSScriptRoot
$Env:PATH = "$Root\vendor\mingw32\mingw32\bin;$Env:PATH"

$Gcc     = "$Root\vendor\mingw32\mingw32\bin\gcc.exe"
$Windres = "$Root\vendor\mingw32\mingw32\bin\windres.exe"
$Ar      = "$Root\vendor\mingw32\mingw32\bin\ar.exe"
$Py      = "$env:DSH_PYTHON"
if (-not $Py -or -not (Test-Path $Py)) { $Py = 'python' }

$Bear    = "$Root\vendor\bearssl-0.6"
$ObjDir  = "$Root\build\obj"
$LibDir  = "$Root\build\lib"
$OutDir  = "$Root\build\out"
$DistDir = "$Root\build\dist"

$CFlags = @(
    '-O2', '-march=i586', '-mno-sse', '-mno-sse2',
    '-D__USE_MINGW_ANSI_STDIO=0',
    '-Wall', '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
    '-Wno-misleading-indentation', '-Wno-format-truncation',
    '-finput-charset=UTF-8', '-fexec-charset=GBK',
    '-D_WIN32_WINNT=0x0501', '-DWINVER=0x0501',
    '-DUNICODE_NONE=1',
    "-I$Root\src\common", "-I$Bear\inc"
)
$LdFlags = @('-static', '-static-libgcc', '-s')
$GuiSub  = '-Wl,--subsystem,windows:4.0'
$ConSub  = '-Wl,--subsystem,console:4.0'

function Log($msg) { Write-Host "  $msg" -ForegroundColor Gray }
$script:HeaderStamp = (Get-ChildItem "$Root\src\common\*.h","$Root\src\agent\*.h" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime

function Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Warn($msg) { Write-Host "  ! $msg" -ForegroundColor Yellow }

function Compile($src, $obj, $extra) {
    # 头文件比目标文件新也要重编：结构体布局变化会让陈旧 .o 与新的
    # App/配置结构错位，造成极难定位的内存踩踏。
    if (Test-Path $obj) {
        $t = (Get-Item $obj).LastWriteTime
        if ($t -gt (Get-Item $src).LastWriteTime -and $t -gt $script:HeaderStamp) { return }
    }
    $argv = @('-c') + $CFlags + $extra + @('-o', $obj, $src)
    $out = & $Gcc @argv 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "编译失败: $src" -ForegroundColor Red
        $out | ForEach-Object { Write-Host "    $_" }
        throw "compile error"
    } elseif ($Verbose -and $out) {
        $out | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkYellow }
    }
}

function Link($objs, $out, $sub, $libs) {
    $argv = @('-o', $out) + $objs + $libs + $LdFlags + @($sub)
    $out2 = & $Gcc @argv 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "链接失败: $out" -ForegroundColor Red
        $out2 | ForEach-Object { Write-Host "    $_" }
        throw "link error"
    }
}

function Res($rc, $obj) {
    $rcDir = Split-Path $rc -Parent
    $rcLeaf = Split-Path $rc -Leaf
    Push-Location $rcDir
    $out = & $Windres -i $rcLeaf -o $obj --codepage=936 2>&1
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -ne 0) {
        Write-Host "资源编译失败: $rc" -ForegroundColor Red
        $out | ForEach-Object { Write-Host "    $_" }
        throw "windres error"
    }
}

# ---------------------------------------------------------------------
# 品牌注入：从 brand.ini 生成 nb_brand.h / brand_res.h / brand_rc.inc
if (-not (Test-Path $Brand)) {
    if (Test-Path "$Root\$Brand") { $Brand = "$Root\$Brand" }
    else { throw "找不到品牌文件: $Brand" }
}
Write-Host "==> 生成品牌头 ($Brand)" -ForegroundColor Cyan
& $Py "$Root\tools\mkbrand.py" $Brand
if ($LASTEXITCODE -ne 0) { throw "mkbrand failed" }

$BrandSum = Get-Content "$Root\src\common\brand_summary.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$ExeName  = "$($BrandSum.exe).exe".ToUpper()

# ---------------------------------------------------------------------
Step "准备目录"
foreach ($d in @($ObjDir, "$ObjDir\bearssl", "$ObjDir\common", "$ObjDir\agent",
                 "$ObjDir\setup", "$ObjDir\keygen", "$ObjDir\tools",
                 $LibDir, $OutDir, $DistDir)) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
}
if ($Clean) {
    Log "清理 build\obj"
    Get-ChildItem "$ObjDir" -Recurse -File | Remove-Item -Force
}

# ---------------------------------------------------------------------
Step "编译 BearSSL (TLS 1.2)"
$bearLib = "$LibDir\libbearssl.a"
if (-not (Test-Path $bearLib)) {
    $objs = @()
    $srcs = Get-ChildItem "$Bear\src" -Recurse -Filter *.c
    $i = 0
    foreach ($s in $srcs) {
        $rel = $s.FullName.Substring($Bear.Length + 1).Replace('\', '_') -replace '\.c$', '.o'
        $o = Join-Path "$ObjDir\bearssl" $rel
        $objs += $o
        if (-not (Test-Path $o)) {
            # 关掉 BearSSL 的 x86 SIMD 加速：目标机器是 486/Pentium/586 时代
            # 的老 PC，SSE2/AES-NI 要么没有、要么这里的 intrinsic 代码会踩内存
            # （实测 br_chacha20_sse2_run 在 ChaCha20-Poly1305 握手时崩溃）。
            # 关掉后自动退回可移植 C 实现。
            $r = & $Gcc -c -O2 "-I$Bear\inc" "-I$Bear\src" -D_WIN32_WINNT=0x0501 -march=i586 -mno-sse -mno-sse2 `
                -DBR_SSE2=0 -DBR_AES_X86NI=0 -DBR_RDRAND=0 -o $o $s.FullName 2>&1
            if ($LASTEXITCODE -ne 0) { Write-Host "bearssl 编译失败: $($s.Name)" -ForegroundColor Red; $r | Select-Object -First 8; throw "bearssl" }
        }
        $i++
    }
    Log "编译了 $i 个 BearSSL 源文件"
    & $Ar rcs $bearLib @objs
    if ($LASTEXITCODE -ne 0) { throw "ar failed" }
}

# ---------------------------------------------------------------------
Step "编译公共模块"
$commonSrc = Get-ChildItem "$Root\src\common\*.c"
$commonObj = @()
foreach ($s in $commonSrc) {
    $o = "$ObjDir\common\$($s.BaseName).o"
    Compile $s.FullName $o @()
    $commonObj += $o
}

# ---------------------------------------------------------------------
Step "生成位图资源"
$artStamp = "$ObjDir\.art"
$artTag = "$($BrandSum.product)|$($BrandSum.company)|$($BrandSum.version)"
$needArt = $true
if ((Test-Path "$Root\src\agent\res\toolbar.bmp") -and (Test-Path $artStamp)) {
    if ((Get-Content $artStamp -Raw).Trim() -eq $artTag) { $needArt = $false }
}
if ($needArt) {
    & $Py "$Root\tools\mkart.py" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "mkart failed" }
    Set-Content $artStamp $artTag -Encoding UTF8
} else {
    Log "位图资源与品牌一致，跳过（删除 build\obj\.art 可强制重画）"
}

# ---------------------------------------------------------------------
function Build-App {
    Step "编译主程序 $ExeName"
    $objs = @()
    foreach ($s in (Get-ChildItem "$Root\src\agent\*.c")) {
        $o = "$ObjDir\agent\$($s.BaseName).o"
        Compile $s.FullName $o @("-I$Root\src\agent")
        $objs += $o
    }
    $resObj = "$ObjDir\agent\res.o"
    Res "$Root\src\agent\res\agent.rc" $resObj
    $objs += $resObj
    Link ($objs + $commonObj) "$OutDir\$ExeName" $GuiSub @(
        $bearLib, '-lcomctl32', '-lcomdlg32', '-lshell32', '-lws2_32',
        '-ladvapi32', '-lole32', '-luuid', '-lgdi32', '-luser32', '-lkernel32')
    Log ((Get-Item "$OutDir\$ExeName").Length.ToString() + " 字节")
}

function Build-Keygen {
    Step "编译 CDK 产生器 CDKEY.EXE"
    $objs = @()
    foreach ($s in (Get-ChildItem "$Root\src\keygen\*.c")) {
        $o = "$ObjDir\keygen\$($s.BaseName).o"
        Compile $s.FullName $o @("-I$Root\src\keygen")
        $objs += $o
    }
    $resObj = "$ObjDir\keygen\res.o"
    Res "$Root\src\keygen\res\keygen.rc" $resObj
    $objs += $resObj
    Link ($objs + $commonObj) "$OutDir\CDKEY.EXE" $GuiSub @(
        $bearLib, '-lcomctl32', '-lcomdlg32', '-lshell32', '-lws2_32', '-ladvapi32',
        '-lgdi32', '-luser32', '-lkernel32')
    Log ((Get-Item "$OutDir\CDKEY.EXE").Length.ToString() + " 字节")
}

function Build-Setup {
    Step "编译安装程序 SETUP.EXE"
    $objs = @()
    foreach ($s in (Get-ChildItem "$Root\src\setup\*.c")) {
        $o = "$ObjDir\setup\$($s.BaseName).o"
        Compile $s.FullName $o @("-I$Root\src\setup")
        $objs += $o
    }
    $resObj = "$ObjDir\setup\res.o"
    Res "$Root\src\setup\res\setup.rc" $resObj
    $objs += $resObj
    # 发布用 SETUP.EXE 不带应用程序清单：Windows XP 的 SxS 解析器遇到
    # <compatibility><supportedOS> 这类 Vista+ 段会直接让 CreateProcess 失败
    # （报“参数不正确”，进程根本不启动）。这里用 setup_nm.rc 链发布版。
    $resNm = "$ObjDir\setup\res_nm.o"
    Res "$Root\src\setup\res\setup_nm.rc" $resNm
    Link (($objs -ne $resObj) + $resNm + $commonObj) "$OutDir\SETUP.EXE" $GuiSub @(
        $bearLib, '-lcomctl32', '-lcomdlg32', '-lshell32', '-lws2_32',
        '-ladvapi32', '-lole32', '-luuid', '-lgdi32', '-luser32', '-lkernel32')
    Log ((Get-Item "$OutDir\SETUP.EXE").Length.ToString() + " 字节")
    # 带 asInvoker 清单的版本只留在 build\out 里给自动化测试用（在本机跑不弹 UAC）
    Link ($objs + $commonObj) "$OutDir\SETUPAV.EXE" $GuiSub @(
        $bearLib, '-lcomctl32', '-lcomdlg32', '-lshell32', '-lws2_32',
        '-ladvapi32', '-lole32', '-luuid', '-lgdi32', '-luser32', '-lkernel32')
    Log ((Get-Item "$OutDir\SETUPAV.EXE").Length.ToString() + " 字节 (测试用)")
}

function Build-ComputerUse {
    Step "编译 Computer Use 工具 NBCU.EXE"
    $cuObj = "$ObjDir\tools\NBCU.o"
    Compile "$Root\src\tools\NBCU.c" $cuObj @("-I$Root\src\tools")
    Link @($cuObj) "$OutDir\NBCU.EXE" $ConSub @(
        '-lgdi32', '-luser32', '-lkernel32')
    Log ((Get-Item "$OutDir\NBCU.EXE").Length.ToString() + " 字节")
}

function Build-Tools {
    Step "编译附加工具"
    foreach ($name in @('CERTUPD', 'SYSINFO', 'UNINST', 'NB-SP1')) {
        $dir = "$Root\src\tools"
        $src = "$dir\$name.c"
        if (-not (Test-Path $src)) { Warn "跳过 $name（缺源码）"; continue }
        $o = "$ObjDir\tools\$name.o"
        Compile $src $o @("-I$Root\src\tools")
        $resObj = "$ObjDir\tools\$name.res.o"
        $rc = "$dir\res\$name.rc"
        $objs = @($o)
        if (Test-Path $rc) { Res $rc $resObj; $objs += $resObj }
        Link ($objs + $commonObj) "$OutDir\$name.EXE" $GuiSub @(
            $bearLib, '-lcomctl32', '-lcomdlg32', '-lshell32', '-lws2_32',
            '-ladvapi32', '-lole32', '-luuid', '-lgdi32', '-luser32', '-lkernel32')
        Log "$name.EXE  $((Get-Item "$OutDir\$name.EXE").Length) 字节"
    }
}

function Build-Docs {
    Step "生成帮助文件 $($BrandSum.exe).CHM"
    & $Py "$Root\tools\mkhelp.py"
    if ($LASTEXITCODE -ne 0) { throw "mkhelp failed" }
    & $Py "$Root\tools\mkchm.py" --src "$Root\build\help" --out "$OutDir\$($BrandSum.exe).CHM" `
        --title "$($BrandSum.product) Help" --default index.htm `
        --toc toc.hhc --index index.hhk
    if ($LASTEXITCODE -ne 0) { throw "mkchm failed" }
    Log ((Get-Item "$OutDir\$($BrandSum.exe).CHM").Length.ToString() + " 字节")
}

function Build-Dist {
    Step "组装发行盘与光盘镜像"
    & $Py "$Root\tools\mkdist.py"
    if ($LASTEXITCODE -ne 0) { throw "mkdist failed" }
}

switch ($Target) {
    'agent'   { Build-App }
    'keygen'  { Build-Keygen }
    'setup'   { Build-Setup }
    'tools'   { Build-Tools; Build-ComputerUse }
    'docs'    { Build-Docs }
    'dist'    { Build-Dist }
    'all'     { Build-App; Build-Keygen; Build-Setup; Build-Tools; Build-ComputerUse }
    'release' { Build-App; Build-Keygen; Build-Setup; Build-Tools; Build-ComputerUse; Build-Docs; Build-Dist }
    default   { throw "未知目标: $Target" }
}

Step "完成"
Get-ChildItem $OutDir -Filter *.EXE | ForEach-Object { Log ("{0}  {1}" -f $_.Name, $_.Length) }
