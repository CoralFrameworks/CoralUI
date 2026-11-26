# ==================================================================================================
# CoralUI Windows build
# ==================================================================================================
# Configures, builds and tests this repository on Windows without a Developer Command Prompt: the
# script finds CMake in a Visual Studio installation when it is not on PATH and selects the newest
# installed Visual Studio generator.
#
#   .\scripts\build-windows.ps1                       # examples only (what a demo needs)
#   .\scripts\build-windows.ps1 -Tests                # framework libraries, unit suites and examples
#   .\scripts\build-windows.ps1 -Configuration Debug -Clean
#   .\scripts\build-windows.ps1 -ThirdParty D:\deps\third_party
#
# The shared libz/libyuv/libgtest tree is resolved automatically from this checkout's third_party/,
# CORAL_SHARED_THIRD_PARTY_DIR, -ThirdParty, or an application checkout beside this workspace (the
# workgroup layout used by the Coral application). Pass -ThirdParty when it lives somewhere else.
# CoralCore and CoralGraphics are taken from external/ when present, otherwise from sibling
# checkouts (../CoralCore and ../CoralGraphics).
# ==================================================================================================

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$Tests,
    [switch]$Clean,
    [string]$BuildDir = '',
    [string]$ThirdParty = '',
    [string]$CMake = '',
    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'

if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Write-Step([string]$Message) {
    Write-Host $Message
}

function Fail([string]$Message) {
    throw "error: $Message"
}

function Invoke-Native {
    param([string[]]$Arguments)

    if ($Arguments.Count -eq 0) {
        Fail 'empty command'
    }
    $command = $Arguments[0]
    $rest = @()
    if ($Arguments.Count -gt 1) {
        $rest = $Arguments[1..($Arguments.Count - 1)]
    }
    & $command @rest
    if ($LASTEXITCODE -ne 0) {
        Fail "$($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

# --------------------------------------------------------------------------------------------------
# Toolchain
# --------------------------------------------------------------------------------------------------
function Resolve-CMake {
    if ($CMake) {
        if (-not (Test-Path -LiteralPath $CMake)) {
            Fail "-CMake '$CMake' does not exist"
        }
        return (Resolve-Path -LiteralPath $CMake).Path
    }

    $onPath = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($onPath) {
        return $onPath.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $instances = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        foreach ($instance in @($instances)) {
            if ([string]::IsNullOrWhiteSpace($instance)) {
                continue
            }
            $candidate = Join-Path $instance.Trim() 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    Fail 'cmake was not found on PATH or in a Visual Studio installation; pass -CMake <path>'
}

function Resolve-Generator([string]$CMakePath) {
    $help = & $CMakePath --help
    foreach ($line in @($help)) {
        if ($line -match '^\s*(\*)?\s*Visual Studio (\d+) (\d{4})\s*=') {
            return "Visual Studio $($Matches[2]) $($Matches[3])"
        }
    }
    Fail 'no Visual Studio generator is available; install the MSVC v143/v144 toolset'
}

# --------------------------------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------------------------------
function Test-ThirdParty([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }
    return (Test-Path -LiteralPath (Join-Path $Path 'libz\coral\CMakeLists.txt')) -and
           (Test-Path -LiteralPath (Join-Path $Path 'libgtest\coral\CMakeLists.txt'))
}

function Resolve-ThirdParty {
    if ($ThirdParty) {
        if (-not (Test-ThirdParty $ThirdParty)) {
            Fail "-ThirdParty '$ThirdParty' has no libz/coral or libgtest/coral; pass the directory that contains both"
        }
        return (Resolve-Path -LiteralPath $ThirdParty).Path
    }

    if ($env:CORAL_SHARED_THIRD_PARTY_DIR -and (Test-ThirdParty $env:CORAL_SHARED_THIRD_PARTY_DIR)) {
        return (Resolve-Path -LiteralPath $env:CORAL_SHARED_THIRD_PARTY_DIR).Path
    }

    $bundled = Join-Path $Root 'third_party'
    if (Test-ThirdParty $bundled) {
        return $bundled
    }

    # Workgroup layout: an application checkout such as D:\Workspace\Coral carries the shared
    # third_party tree beside this workspace (D:\Workspace\CoralFrameworks\CoralUI). Probe the
    # workspace root's immediate children for one that has the tree.
    $workspace = Split-Path -Parent (Split-Path -Parent $Root)
    if ($workspace -and (Test-Path -LiteralPath $workspace)) {
        foreach ($entry in Get-ChildItem -LiteralPath $workspace -Directory -ErrorAction SilentlyContinue) {
            $candidate = Join-Path $entry.FullName 'third_party'
            if (Test-ThirdParty $candidate) {
                return $candidate
            }
        }
    }

    Fail 'no shared third_party tree (libz + libgtest) was found. Initialize the submodules with ' +
         '"git submodule update --init --recursive", pass -ThirdParty <dir>, or set ' +
         'CORAL_SHARED_THIRD_PARTY_DIR.'
}

# --------------------------------------------------------------------------------------------------
# Configure, build, test
# --------------------------------------------------------------------------------------------------
$CMakePath = Resolve-CMake
$Generator = Resolve-Generator $CMakePath
$ThirdPartyPath = Resolve-ThirdParty

$Source = if ($Tests) { $Root } else { Join-Path $Root 'examples' }
$BuildPath = if ($BuildDir) {
    if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $Root $BuildDir }
} elseif ($Tests) {
    Join-Path $Root 'out\windows-x64'
} else {
    Join-Path $Root 'out\examples'
}

if ($Clean -and (Test-Path -LiteralPath $BuildPath)) {
    Write-Step "Removing $BuildPath"
    Remove-Item -LiteralPath $BuildPath -Recurse -Force
}

Write-Step "CMake:     $CMakePath"
Write-Step "Generator: $Generator"
Write-Step "Third party: $ThirdPartyPath"
Write-Step "Source:    $Source"
Write-Step "Build:     $BuildPath"

if (-not (Test-Path -LiteralPath (Join-Path $Root 'external\CoralCore\CMakeLists.txt')) -and
    -not (Test-Path -LiteralPath (Join-Path $Root '..\CoralCore\CMakeLists.txt'))) {
    Fail 'CoralCore was not found under external/ or ..\CoralCore; pass -DCORAL_CORE_DIR through a manual cmake configure'
}
if (-not (Test-Path -LiteralPath (Join-Path $Root 'external\CoralGraphics\CMakeLists.txt')) -and
    -not (Test-Path -LiteralPath (Join-Path $Root '..\CoralGraphics\CMakeLists.txt'))) {
    Fail 'CoralGraphics was not found under external/ or ..\CoralGraphics; pass -DCORAL_GRAPHICS_DIR through a manual cmake configure'
}

$configure = @(
    $CMakePath, '-S', $Source, '-B', $BuildPath,
    '-G', $Generator, '-A', 'x64',
    "-DCORAL_SHARED_THIRD_PARTY_DIR=$ThirdPartyPath"
)
if ($Tests) {
    $configure += '-DCORAL_BUILD_TESTS=ON'
} else {
    $configure += '-DCORAL_BUILD_TESTS=OFF'
}
Write-Step 'Configuring'
Invoke-Native -Arguments $configure

$parallel = if ($Jobs -gt 0) { @('--parallel', "$Jobs") } else { @('--parallel') }
Write-Step 'Building'
$build = @($CMakePath, '--build', $BuildPath, '--config', $Configuration) + $parallel
if ($Tests) {
    $build += @('--target', 'test_ui', 'ui_native_smoke', 'ui_multimonitor_smoke',
        'crash_windows_smoke', 'ui_demo', 'ui_mvvm_demo', 'ui_dsl_gallery', 'ui_dsl_bench')
}
Invoke-Native -Arguments $build

$ctest = Join-Path (Split-Path -Parent $CMakePath) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) {
    $ctest = 'ctest'
}
Write-Step 'Running tests'
if ($Tests) {
    $testFilter = '^(test_ui|ui_native_smoke|ui_multimonitor_smoke|crash_windows_smoke|example_)'
} else {
    $testFilter = 'example_'
}
Invoke-Native -Arguments @($ctest, '--test-dir', $BuildPath, '-C', $Configuration,
    '-R', $testFilter, '--output-on-failure')

Write-Step ''
Write-Step "Done. The build is in $BuildPath"
