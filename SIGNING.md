# Code signing policy

WinHTTrack's Windows binaries and its installer carry an Authenticode signature, so that
Windows names a verified publisher instead of warning you about an unknown one.

## Attribution

Free code signing provided by [SignPath.io](https://signpath.io), certificate by
[SignPath Foundation](https://signpath.org).

The certificate belongs to SignPath Foundation, not to HTTrack or to its author. SignPath
is not a certificate authority, so it cannot issue a certificate in anyone else's name.
Windows will therefore show *SignPath Foundation* as the publisher of a signed WinHTTrack
installer. That is expected, and many other open-source projects sign the same way.

## What is signed

Everything this project's CI builds and ships:

| File | What it is |
| --- | --- |
| `WinHTTrack.exe` | the GUI |
| `httrack.exe` | the command-line version |
| `libhttrack.dll` | the engine, built from [xroche/httrack](https://github.com/xroche/httrack) |
| `libssl-*.dll`, `libcrypto-*.dll`, `z.dll` | OpenSSL and zlib, which vcpkg compiles from source during the build |
| `httrack_*_*.exe` | the Inno Setup installer |

The uninstaller that Inno Setup embeds (`unins000.exe`) stays unsigned. Inno can only sign
it by calling out to a `signtool`-style helper while it compiles, and SignPath's submission
API does not work that way. The installer you actually download and double-click is signed,
and that is the file Windows judges.

## Team roles

One person maintains HTTrack, and he holds all three SignPath roles:

| Role | Who |
| --- | --- |
| Author (submits a signing request) | the GitHub Actions CI user, never a human |
| Reviewer | Xavier Roche |
| Approver | Xavier Roche |

Anyone who is not a committer contributes through a pull request, which is reviewed before
it merges. Everyone with write access to the source repositories uses multi-factor
authentication on both GitHub and SignPath.

## How signing happens

Only one thing ever signs a WinHTTrack binary: the
[`windows-build`](.github/workflows/windows-build.yml) workflow, on a GitHub-hosted runner,
in the `xroche/httrack-windows` repository, on a version tag. Nobody can sign one from a
developer's machine. The SignPath project uses origin verification, which means SignPath
pulls the artifact and the build metadata from GitHub's own API instead of believing
whatever the build script tells it, and it does not let interactive users submit artifacts
at all.

Signing takes two steps, because SignPath treats an Inno Setup installer as one opaque
executable and cannot reach the files packed inside it:

1. CI builds the binaries, then submits them to SignPath, which signs them;
2. CI builds the installer from the signed binaries, then submits that, and SignPath signs
   it in turn;
3. CI installs, runs and uninstalls the signed installer, so the file it tests is the file
   you get.

Before either certificate is used, a human approves the request in SignPath's web
interface.

## Verifying a signature

Right-click the installer, choose *Properties*, and open the *Digital Signatures* tab. The
signer should read SignPath Foundation, and the signature should be valid. From PowerShell:

```powershell
Get-AuthenticodeSignature .\httrack_x64_3.49.12.exe | Format-List Status, SignerCertificate
```

`Status` must say `Valid`. The signatures use SHA-256 and carry an RFC 3161 timestamp, so
they survive the expiry of the certificate that made them.

Source releases of the engine are a separate matter: they are signed with the HTTrack PGP
key `rsa4096/60C3AA7180598EFB`, which has nothing to do with Authenticode and is documented
on [httrack.com](https://www.httrack.com/).

## Reporting a problem

Tell us if you think a WinHTTrack binary was signed that should not have been, or you find
a signed binary that does not match the source it claims to come from. See
[SECURITY.md](SECURITY.md). You can also report a serious case straight to SignPath
Foundation, who can revoke the certificate.

## Licence

WinHTTrack is free software under the GNU General Public License v3 or later; see
[COPYING](COPYING). It is not dual-licensed for commercial use.
