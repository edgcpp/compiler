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

if (-not (@($env:Path -split ';' |
    Where-Object { $edgToolDirs -icontains $_ }))) {
  $env:Path = "$($edgToolDirs -join ';');$env:Path"
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

Remove-Variable -Name edgRepoRoot, edgToolDirs, edgPyLibs `
  -ErrorAction SilentlyContinue
