# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Windows counterpart of the ".envrc" file used with direnv on POSIX systems:
# from a PowerShell prompt, dot source this script to make the EDG
# development tools and their supporting Python libraries available in the
# current session:
#
#   . .\dev-env.ps1
#
# This prepends "dev_tools/win-bin" and "dev_tools/bin" to the PATH and
# "dev_tools/pylibs" to the PYTHONPATH.  The script may be dot sourced from
# any directory and repeated dot sourcing is harmless.

$edgRepoRoot = $PSScriptRoot

$edgToolDirs = @(
  (Join-Path $edgRepoRoot 'dev_tools\win-bin'),
  (Join-Path $edgRepoRoot 'dev_tools\bin')
)

# Add each tool directory that is not already on the PATH (preserving the
# order above, so the ".bat" shims take precedence over the extensionless
# scripts).
$edgMissingDirs = @()
foreach ($edgToolDir in $edgToolDirs) {
  $edgOnPath = @($env:Path -split ';' | Where-Object { $_ -ieq $edgToolDir })
  if ($edgOnPath.Count -eq 0) {
    $edgMissingDirs += $edgToolDir
  }
}
if ($edgMissingDirs.Count -gt 0) {
  $env:Path = "$($edgMissingDirs -join ';');$env:Path"
}

$edgPyLibs = Join-Path $edgRepoRoot 'dev_tools\pylibs'
if (-not (@($env:PYTHONPATH -split ';' |
    Where-Object { $_ -ieq $edgPyLibs }))) {
  if ($env:PYTHONPATH) {
    $env:PYTHONPATH = "$edgPyLibs;$env:PYTHONPATH"
  } else {
    $env:PYTHONPATH = $edgPyLibs
  }
}

Remove-Variable -Name edgRepoRoot, edgToolDirs, edgToolDir, edgOnPath, `
  edgMissingDirs, edgPyLibs -ErrorAction SilentlyContinue
