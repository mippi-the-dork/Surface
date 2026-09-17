<#
.SYNOPSIS
Builds a Win64 distribution using the selected Unreal installation's BuildPlugin.
.EXAMPLE
.\PackagePlugin.ps1 -EngineRoot 'F:\Program Files\Epic Games\UE_5.8' -OutputDirectory 'H:\PluginBuilds\Surface'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$pluginRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pluginFile = Join-Path $pluginRoot 'Surface.uplugin'
$uatFile = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$packageRoot = [IO.Path]::GetFullPath($OutputDirectory)

if (-not (Test-Path -LiteralPath $uatFile -PathType Leaf)) {
    throw "RunUAT.bat was not found in the supplied EngineRoot: $EngineRoot"
}
if (-not (Test-Path -LiteralPath $pluginFile -PathType Leaf)) {
    throw "The plugin descriptor was not found: $pluginFile"
}
# UAT can clear its output. Require a new directory outside the source tree.
$sourcePrefix = $pluginRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
if ($packageRoot.Equals($pluginRoot, [StringComparison]::OrdinalIgnoreCase) -or
    $packageRoot.StartsWith($sourcePrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDirectory must be outside the plugin source folder.'
}
if (Test-Path -LiteralPath $packageRoot) {
    throw 'OutputDirectory already exists. Choose a new folder to protect existing files.'
}

$uatArguments = @(
    'BuildPlugin',
    "-Plugin=$pluginFile",
    "-Package=$packageRoot",
    '-TargetPlatforms=Win64',
    '-Rocket',
    '-StrictIncludes'
)
& $uatFile @uatArguments
if ($LASTEXITCODE -ne 0) {
    throw "BuildPlugin failed with exit code $LASTEXITCODE. Review the UAT build log."
}
Write-Output "Packaged plugin: $packageRoot"
