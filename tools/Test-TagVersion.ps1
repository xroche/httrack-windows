#!/usr/bin/env pwsh
# Table test for the tagged-build guard. Runs on every push and PR, where no tag
# exists, so the release path is proven without pushing one.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TagVersion.ps1')

$fail = 0
function Check($what, $got, $want) {
    if ($got -ne $want) { Write-Host "  FAIL $what : got $got, wanted $want"; $script:fail++ }
    else { Write-Host "  ok   $what" }
}

# A header shaped like the real one: the three decoy macros are the point.
$dir = Join-Path ([IO.Path]::GetTempPath()) ("tagver-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $dir | Out-Null
$full = Join-Path $dir 'version.h'
Set-Content -LiteralPath $full -Value @'
#ifndef WINHTTRACK_VERSION_H
#define WINHTTRACK_VERSION_H
#define WINHTTRACK_VERSION "3.50-beta-7"
#define WINHTTRACK_VERSIONID "3.49.99.7"
#define WINHTTRACK_VERSION_NUM 3, 49, 99, 7
#endif
'@

Write-Host "--- Get-GuiVersion binds to the right macro ---"
Check 'picks WINHTTRACK_VERSION, not VERSIONID/NUM/_H' (Get-GuiVersion -Path $full) '3.50-beta-7'

Write-Host "--- Get-GuiVersion refuses what it cannot read ---"
$bare = Join-Path $dir 'novers.h'
Set-Content -LiteralPath $bare -Value '#define WINHTTRACK_VERSIONID "3.49.99.7"'
foreach ($case in @(@{n='absent macro'; p=$bare}, @{n='missing file'; p=(Join-Path $dir 'nope.h')})) {
    $threw = $false
    try { Get-GuiVersion -Path $case.p | Out-Null } catch { $threw = $true }
    Check $case.n $threw $true
}

Write-Host "--- Test-TagMatchesVersion ---"
$v = '3.50-beta-7'
$cases = @(
    @{ tag = '3.50-beta-7';        want = $true;  why = 'exact match' }
    @{ tag = '3.50-beta-6';        want = $false; why = 'the number this round nearly shipped' }
    @{ tag = 'v3.50-beta-7';       want = $true;  why = 'NEGATIVE CONTROL: deliberately wrong, must fail' }
    @{ tag = '3.05-beta-7';        want = $false; why = 'transposed digits' }
    @{ tag = '3.50-BETA-7';        want = $false; why = 'case differs' }
    @{ tag = '3.50-beta-70';       want = $false; why = 'superstring' }
    @{ tag = '3.50-beta-';         want = $false; why = 'prefix' }
    @{ tag = '3.50-beta-7 ';       want = $false; why = 'trailing space' }
    @{ tag = ' 3.50-beta-7';       want = $false; why = 'leading space' }
    @{ tag = '';                   want = $false; why = 'empty tag' }
    # Ordinal, not ICU: -ceq treats a default-ignorable code point as equal and would pass this.
    @{ tag = "3.50-beta-7$([char]0x00AD)"; want = $false; why = 'trailing soft hyphen' }
)
foreach ($c in $cases) { Check $c.why (Test-TagMatchesVersion -Tag $c.tag -Version $v) $c.want }

Remove-Item -Recurse -Force $dir
if ($fail) { throw "$fail tag/version case(s) failed" }
Write-Host "::notice::tag/version guard: $($cases.Count + 3) cases pass"
