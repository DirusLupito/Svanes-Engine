[CmdletBinding()]
param()

$repoRoot = Split-Path -Parent $PSScriptRoot
$ok = $true

$installCmake = 'winget install --id Kitware.CMake -e'
$installCompiler = 'Install Visual Studio 2026 or Visual Studio 2022 with the Desktop development with C++ workload'
$installCompilerCmd = 'winget install --id Microsoft.VisualStudio.2026.Community -e --override "--add Microsoft.VisualStudio.Workload.NativeDesktop"'

Write-Host 'Checking dependencies...'

try {
    $cmakeVersionLine = & (Join-Path $PSScriptRoot 'run-cmake.ps1') --version | Select-Object -First 1
    Write-Host "  [ok] cmake found ($cmakeVersionLine)"
}
catch {
    Write-Host "  [missing] cmake: $($_.Exception.Message) -- $installCmake"
    Write-Host "  [missing] compatible MSVC C++ toolset may also be missing -- $installCompiler"
    Write-Host "  [missing] to install MSVC C++ toolset via winget, run: $installCompilerCmd"
    $ok = $false
}

$thirdparty = [ordered] @{
    'SDL3' = Join-Path $repoRoot 'thirdparty\sdl3-src'
    'SDL3_image' = Join-Path $repoRoot 'thirdparty\sdl3_image-src'
}

foreach ($name in $thirdparty.Keys) {
    $path = $thirdparty[$name]
    if (Test-Path -LiteralPath $path) {
        Write-Host "  [ok] $name found (thirdparty/$(Split-Path -Leaf $path))"
    }
    else {
        Write-Host "  [missing] $name not fetched yet -- run 'just fetch-deps'"
        $ok = $false
    }
}

if (-not $ok) {
    exit 1
}

Write-Host 'All dependencies present.'
