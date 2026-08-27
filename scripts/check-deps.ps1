[CmdletBinding()]
param()

$repoRoot = Split-Path -Parent $PSScriptRoot
$ok = $true

Write-Host 'Checking dependencies...'

try {
    $cmakeVersionLine = & (Join-Path $PSScriptRoot 'run-cmake.ps1') --version | Select-Object -First 1
    Write-Host "  [ok] cmake found ($cmakeVersionLine)"
}
catch {
    Write-Host "  [missing] cmake: $($_.Exception.Message)"
    $ok = $false
}

$thirdparty = @{
    'SDL3' = Join-Path $repoRoot 'thirdparty\sdl3-src'
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
