# Compiles the mod with Windhawk's bundled toolchain, using the same flags as
# the Windhawk editor. Only checks that the mod builds: install it through the
# Windhawk UI ("Create a new mod", paste the source, "Compile Mod").
param(
    [string]$Source = "$PSScriptRoot\komorebi-workspaces.wh.cpp",
    [string]$WindhawkRoot = "C:\Program Files\Windhawk"
)

$ErrorActionPreference = "Stop"

$ini = Get-Content "$WindhawkRoot\windhawk.ini" -Raw
$enginePath = [regex]::Match($ini, "EnginePath=(.+)").Groups[1].Value.Trim()
$compiler = "$WindhawkRoot\Compiler"
$lib = "$WindhawkRoot\$enginePath\64\windhawk.lib"

$text = Get-Content $Source -Raw
$id = [regex]::Match($text, "@id\s+(\S+)").Groups[1].Value
$version = [regex]::Match($text, "@version\s+(\S+)").Groups[1].Value
$options = [regex]::Match($text, "@compilerOptions\s+(.+)").Groups[1].Value.Trim() -split "\s+"

$outDir = "$PSScriptRoot\build"
New-Item -ItemType Directory -Force $outDir | Out-Null
$out = "$outDir\$id.dll"

$clangArgs = @(
    "-std=c++23", "-O2", "-shared", "-DUNICODE", "-D_UNICODE",
    "-DWINVER=0x0A00", "-D_WIN32_WINNT=0x0A00", "-D_WIN32_IE=0x0A00",
    "-DNTDDI_VERSION=0x0A000008", "-D__USE_MINGW_ANSI_STDIO=0", "-DWH_MOD",
    # Windows PowerShell 5.1 drops unescaped embedded quotes of native args.
    "-DWH_MOD_ID=L\`"$id\`"", "-DWH_MOD_VERSION=L\`"$version\`"",
    $lib, "-x", "c++", $Source, "-include", "windhawk_api.h",
    "-target", "x86_64-w64-mingw32", "-Wl,--export-all-symbols",
    "-o", $out
) + $options

Push-Location $compiler
try {
    $ErrorActionPreference = "Continue"
    & "$compiler\bin\clang++.exe" @clangArgs 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}

Write-Host "OK: $out"
