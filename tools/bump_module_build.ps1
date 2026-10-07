# Module build version bump -- run by the MSBuild target BumpModuleBuildCount in each module .vcxproj.
#
# A module's build version is "<VimCommon __VERSION>.<build count>". The count lives in the module's tracked
# ModuleBuildVersion.h and goes up by one for every build in which the module's sources changed (the MSBuild
# target is skipped by Inputs/Outputs when nothing changed). It restarts at 1 whenever the __VERSION this module
# is built against changes. The module exports the string as __GetModuleBuildVersion, and the core's two module
# loaders log it at load time.
#
# Every module repository carries an identical copy of this script in its tools directory; keep the copies
# identical. Pure ASCII and CRLF on purpose.
param(
	[Parameter(Mandatory = $true)][string]$Header,
	[Parameter(Mandatory = $true)][string]$VimCommon,
	[Parameter(Mandatory = $true)][string]$Stamp
)
$ErrorActionPreference = 'Stop'

$m = Select-String -Path $VimCommon -Pattern '^\s*#define\s+__VERSION\s+"([^"]+)"' | Select-Object -First 1
if (-not $m) { Write-Host "module-build: ERROR - no __VERSION define found in $VimCommon"; exit 1 }
$core = $m.Matches[0].Groups[1].Value

$bound = ''
$count = 0
if (Test-Path $Header) {
	$t = [IO.File]::ReadAllText($Header)
	if ($t -match '#define\s+VM_MODULE_BOUND_CORE\s+"([^"]+)"') { $bound = $Matches[1] }
	if ($t -match '#define\s+VM_MODULE_BUILD_COUNT_STR\s+"(\d+)"') { $count = [int]$Matches[1] }
}
if ($bound -ne $core) { $count = 1 } else { $count = $count + 1 }

$lines = @(
	'// Module build version = "<VimCommon __VERSION>.<build count>", exported as __GetModuleBuildVersion and',
	'// logged by the core module loaders. REWRITTEN BY THE BUILD (tools/bump_module_build.ps1, MSBuild target',
	'// BumpModuleBuildCount) whenever this module''s sources changed; the count restarts at 1 when __VERSION',
	'// changes. Do not edit by hand. Tracked on purpose so the count is shared.',
	'#pragma once',
	"#define VM_MODULE_BOUND_CORE `"$core`"",
	"#define VM_MODULE_BUILD_COUNT_STR `"$count`""
)
$enc = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($Header, ($lines -join "`r`n") + "`r`n", $enc)
$stampDir = Split-Path -Parent $Stamp
if (-not (Test-Path $stampDir)) { New-Item -ItemType Directory -Force $stampDir | Out-Null }
[IO.File]::WriteAllText($Stamp, "$core.$count`r`n", $enc)
Write-Host ("module-build: " + (Split-Path -Leaf (Split-Path -Parent $Header)) + " -> $core.$count")
exit 0
