# Shared by the tagged-build guard and its test, so the release path and the table
# that proves it can never drift apart.

# The GUI version string from WinHTTrack/version.h. Throws if the macro is unreadable:
# a silent empty result would compare unequal and read as a mistyped tag.
function Get-GuiVersion {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { throw "no version header at $Path" }
    $vh = Get-Content -LiteralPath $Path -Raw
    # \s+ cannot match the _ or I that follow, so VERSION_H, VERSIONID and VERSION_NUM never bind.
    if ($vh -notmatch '#define\s+WINHTTRACK_VERSION\s+"([^"]+)"') { throw "cannot read WINHTTRACK_VERSION from $Path" }
    return $Matches[1]
}

# Ordinal, not -ceq: PowerShell's case-sensitive operators compare through ICU, which
# ignores default-ignorable code points, so a tag carrying a soft hyphen would pass.
function Test-TagMatchesVersion {
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Tag,
          [Parameter(Mandatory)][AllowEmptyString()][string]$Version)
    return [string]::Equals($Tag, $Version, [StringComparison]::Ordinal)
}
