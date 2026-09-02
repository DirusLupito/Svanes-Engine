[CmdletBinding()]
param(
    [switch] $ResolveWindowsPreset,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $CMakeArguments
)

$minimumVersion = [Version] '3.25.0'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$toolchainSpecs = @(
    [PSCustomObject]@{
        ConfigurePreset = 'windows-msvc'
        Generator = 'Visual Studio 18 2026'
        VersionRange = '[18.0,19.0)'
    },
    [PSCustomObject]@{
        ConfigurePreset = 'windows-vs2022'
        Generator = 'Visual Studio 17 2022'
        VersionRange = '[17.0,18.0)'
    }
)
$presetToConfigurePreset = @{
    'windows-msvc' = 'windows-msvc'
    'windows-debug' = 'windows-msvc'
    'windows-release' = 'windows-msvc'
    'windows-vs2022' = 'windows-vs2022'
    'windows-vs2022-debug' = 'windows-vs2022'
    'windows-vs2022-release' = 'windows-vs2022'
}

function Get-CMakeCapabilities {
    param([string] $Path)

    try {
        $json = & $Path -E capabilities 2>$null
        if ($LASTEXITCODE -ne 0 -or -not $json) {
            return $null
        }

        return $json | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Get-CMakeCandidates {
    param([string] $VisualStudioPath)

    $bundledCmakePath = Join-Path $VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $pathCMakePaths = @(
        Get-Command cmake.exe -All -ErrorAction SilentlyContinue |
            ForEach-Object { $_.Source }
    )

    @($bundledCmakePath) + $pathCMakePaths |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -Unique
}

function Resolve-WindowsToolchain {
    param([string] $ConfigurePreset)

    if (-not (Test-Path -LiteralPath $vswherePath)) {
        throw 'Visual Studio Installer was not found. Install Visual Studio 2026 or 2022 with the Desktop development with C++ workload.'
    }

    $specs = $toolchainSpecs
    if ($ConfigurePreset) {
        $specs = @($toolchainSpecs | Where-Object { $_.ConfigurePreset -eq $ConfigurePreset })
        if ($specs.Count -eq 0) {
            throw "Unknown Windows configure preset '$ConfigurePreset'."
        }
    }

    # iterate over all the msvc versions and see
    # if we can find a compatible cmake for any of them
    foreach ($spec in $specs) {
        $visualStudioPath = & $vswherePath `
            -latest `
            -products '*' `
            -version $spec.VersionRange `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath |
            Select-Object -First 1

        if (-not $visualStudioPath) {
            continue
        }

        foreach ($candidate in @(Get-CMakeCandidates -VisualStudioPath $visualStudioPath)) {
            $capabilities = Get-CMakeCapabilities -Path $candidate
            if (-not $capabilities) {
                continue
            }

            $version = [Version]::new(
                [int] $capabilities.version.major,
                [int] $capabilities.version.minor,
                [int] $capabilities.version.patch
            )
            $supportsGenerator = @($capabilities.generators.name) -contains $spec.Generator
            if ($version -ge $minimumVersion -and $supportsGenerator) {
                return [PSCustomObject]@{
                    CMakePath = $candidate
                    ConfigurePreset = $spec.ConfigurePreset
                    Generator = $spec.Generator
                    VisualStudioPath = $visualStudioPath
                }
            }
        }
    }

    $requestedDescription = if ($ConfigurePreset) {
        "the '$ConfigurePreset' preset"
    }
    else {
        'Visual Studio 2026 or 2022'
    }
    throw "No compatible CMake/Visual Studio pair was found for $requestedDescription. CMake $minimumVersion or newer and the Desktop development with C++ workload are required."
}

function Get-PresetName {
    param([string[]] $Arguments)

    for ($index = 0; $index -lt $Arguments.Count; $index++) {
        if ($Arguments[$index] -eq '--preset' -and $index + 1 -lt $Arguments.Count) {
            return $Arguments[$index + 1]
        }

        if ($Arguments[$index].StartsWith('--preset=')) {
            return $Arguments[$index].Substring('--preset='.Length)
        }
    }

    return $null
}

if ($ResolveWindowsPreset) {
    if ($CMakeArguments.Count -gt 0) {
        throw '-ResolveWindowsPreset does not accept CMake arguments.'
    }

    $toolchain = Resolve-WindowsToolchain
    Write-Output $toolchain.ConfigurePreset
    exit 0
}

$requestedConfigurePreset = $null
$presetName = Get-PresetName -Arguments $CMakeArguments
if ($presetName) {
    if ($presetToConfigurePreset.ContainsKey($presetName)) {
        $requestedConfigurePreset = $presetToConfigurePreset[$presetName]
    }
    elseif ($presetName.StartsWith('windows-')) {
        throw "Windows preset '$presetName' is not mapped to a Visual Studio toolchain in scripts/run-cmake.ps1."
    }
}

$toolchain = Resolve-WindowsToolchain -ConfigurePreset $requestedConfigurePreset

# apparently the following can happen:
# Some terminal hosts supply an uppercase PATH entry while MSBuild adds a
# title-cased Path entry. Older .NET process APIs reject an environment 
# block containing both spellings.

# So we normalize it before MSBuild launches cl.exe. 
$normalizedPath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $normalizedPath

& $toolchain.CMakePath @CMakeArguments
exit $LASTEXITCODE
