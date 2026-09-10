<#
    Packages a finished AETHER Release build into dist\AETHER_Setup_1.0.0.exe.

    Run this after building the Release VST3 target, or just run
    make_installer.bat in the project root, which does both.

        powershell -ExecutionPolicy Bypass -File installer\build-installer.ps1 -BuildDir build\windows-vs2026

    The result is one self-contained native Win32 executable: no .NET, no runtime
    dependencies, no administrator rights. It also serves as its own uninstaller.
#>
[CmdletBinding()]
param(
    [string]$BuildDir = 'build\windows-vs2026',
    [string]$Configuration = 'Release',
    [string]$Generator = ''
)

$ErrorActionPreference = 'Stop'
$version   = '1.0.0'
$repoRoot  = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$artefacts = Join-Path $buildRoot "AETHER_artefacts\$Configuration"
$vst3      = Join-Path $artefacts 'VST3\AETHER.vst3'
$distRoot  = Join-Path $repoRoot 'dist'
$name      = "AETHER_Setup_${version}.exe"
$target    = Join-Path $distRoot $name

if (-not (Test-Path -LiteralPath $vst3)) { throw "VST3 bundle not found: $vst3`nBuild the Release configuration first." }

# ---- 1. Stage the payload -------------------------------------------------
$stage   = Join-Path ([System.IO.Path]::GetTempPath()) 'AetherPayloadStage'
$payload = Join-Path $PSScriptRoot 'native\payload.zip'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $stage 'VST3') -Force | Out-Null

Copy-Item -LiteralPath $vst3 -Destination (Join-Path $stage 'VST3') -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $stage -Force

if (Test-Path -LiteralPath $payload) { Remove-Item -LiteralPath $payload -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $payload -CompressionLevel Optimal
Write-Host ("Payload: {0:N1} MB" -f ((Get-Item -LiteralPath $payload).Length / 1MB))

# ---- 2. Build the installer ----------------------------------------------
$installerBuild = Join-Path $buildRoot 'installer'
$cmakeArgs = @('-S', (Join-Path $PSScriptRoot 'native'), '-B', $installerBuild, "-DPAYLOAD_ZIP=$payload")
if ($Generator) { $cmakeArgs += @('-G', $Generator) }
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for the installer ($LASTEXITCODE)." }
& cmake --build $installerBuild --config Release
if ($LASTEXITCODE -ne 0) { throw "Installer build failed ($LASTEXITCODE)." }

$built = Get-ChildItem -LiteralPath $installerBuild -Recurse -Filter 'AETHER_Setup.exe' | Select-Object -First 1
if ($null -eq $built) { throw 'AETHER_Setup.exe was not produced.' }

# ---- 3. Publish ----------------------------------------------------------
New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
Copy-Item -LiteralPath $built.FullName -Destination $target -Force
Remove-Item -LiteralPath $stage -Recurse -Force

$hash = Get-FileHash -LiteralPath $target -Algorithm SHA256
[System.IO.File]::WriteAllText((Join-Path $distRoot ($name + '.sha256')),
    "$($hash.Hash.ToLowerInvariant())  $name`r`n", [System.Text.Encoding]::ASCII)

Write-Host ''
Write-Host "Installer: $target"
Write-Host ("Size:      {0:N1} MB" -f ((Get-Item -LiteralPath $target).Length / 1MB))
Write-Host "SHA256:    $($hash.Hash.ToLowerInvariant())"
