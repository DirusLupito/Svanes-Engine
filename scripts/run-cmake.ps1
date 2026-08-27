[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $CMakeArguments
)

$minimumVersion = [Version] '4.2.0'
$cmakePath = $null

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $versionLine = & $cmakeCommand.Source --version | Select-Object -First 1
    if ($versionLine -match '(\d+\.\d+\.\d+)') {
        $detectedVersion = [Version] $Matches[1]
        if ($detectedVersion -ge $minimumVersion) {
            $cmakePath = $cmakeCommand.Source
        }
    }
}

if (-not $cmakePath) {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswherePath) {
        $visualStudioPath = & $vswherePath `
            -latest `
            -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.CMake.Project `
            -property installationPath

        if ($visualStudioPath) {
            $bundledCmakePath = Join-Path $visualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $bundledCmakePath) {
                $cmakePath = $bundledCmakePath
            }
        }
    }
}

if (-not $cmakePath) {
    throw 'CMake 4.2 or newer was not found. Install the Visual Studio CMake component or a current standalone CMake release.'
}

# Some terminal hosts supply an uppercase PATH entry while MSBuild adds a
# title-cased Path entry. Normalize it before MSBuild launches cl.exe; older
# .NET process APIs reject an environment block containing both spellings.
$normalizedPath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $normalizedPath

& $cmakePath @CMakeArguments
exit $LASTEXITCODE
