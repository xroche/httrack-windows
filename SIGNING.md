# Code signing policy

WinHTTrack's Windows binaries and installer are Authenticode-signed, so that Windows
can show a verified publisher instead of "Unknown publisher" when you run them.

## Attribution

Free code signing provided by [SignPath.io](https://signpath.io), certificate by
[SignPath Foundation](https://signpath.org).

The certificate is issued to **SignPath Foundation**, not to HTTrack or to its author:
SignPath Foundation is not a certificate authority and cannot issue certificates in
someone else's name. Windows will therefore name *SignPath Foundation* as the
publisher of a signed WinHTTrack installer. This is expected, and is the same
arrangement used by many other open-source projects.

## What is signed

Every PE built by this project's CI and shipped to users:

| File | What it is |
| --- | --- |
| `WinHTTrack.exe` | the GUI |
| `httrack.exe` | the command-line version |
| `libhttrack.dll` | the engine, built from [xroche/httrack](https://github.com/xroche/httrack) |
| `libssl-*.dll`, `libcrypto-*.dll`, `z.dll` | OpenSSL and zlib, compiled from source by vcpkg during the build |
| `httrack_*_*.exe` | the Inno Setup installer |

The installer's embedded uninstaller (`unins000.exe`) is **not** signed: Inno Setup can
only sign it through a synchronous `signtool`-style helper at compile time, which the
SignPath submission API is not. The installer you download and run is signed, which is
the file Windows judges.

## Team roles

HTTrack is maintained by a single person, and all three SignPath roles are held by him:

| Role | Who |
| --- | --- |
| Author (submits a signing request) | the GitHub Actions CI user — never a human |
| Reviewer | Xavier Roche |
| Approver | Xavier Roche |

Contributions from anyone who is not a committer arrive as pull requests and are
reviewed before merge. Everyone with write access to the source repositories has
multi-factor authentication enabled on both GitHub and SignPath.

## How signing happens

Signing is only ever performed by the
[`windows-build`](.github/workflows/windows-build.yml) workflow, running on
GitHub-hosted runners, from the `xroche/httrack-windows` repository, on a version tag.
It cannot be performed from a developer's machine: the SignPath project uses origin
verification, so SignPath fetches the artifact and the build metadata from GitHub's API
itself rather than trusting anything the build script asserts, and interactive users are
not permitted to submit artifacts.

Signing is a two-step process, because SignPath signs an Inno Setup installer only as an
opaque executable and cannot reach the files packaged inside it:

1. the binaries are built, then submitted to SignPath and signed;
2. the installer is built **from the signed binaries**, then submitted and signed in
   turn;
3. the signed installer is then installed, run and uninstalled by CI, so that what is
   tested is the exact file that is published.

Each release is approved by a human in the SignPath web interface before its
certificate is used.

## Verifying a signature

Right-click the installer, choose *Properties*, and open the *Digital Signatures* tab;
the signer should read **SignPath Foundation** and the signature should be marked valid.
From PowerShell:

```powershell
Get-AuthenticodeSignature .\httrack_x64_3.49.12.exe | Format-List Status, SignerCertificate
```

`Status` must be `Valid`. Signatures are SHA-256 and carry an RFC 3161 timestamp, so
they remain valid after the signing certificate expires.

Source releases of the engine are separately signed with the HTTrack PGP key
`rsa4096/60C3AA7180598EFB`; that is unrelated to Authenticode and is documented on
[httrack.com](https://www.httrack.com/).

## Reporting a problem

If you believe a WinHTTrack binary has been signed that should not have been, or you
find a signed binary that does not correspond to the source it claims to be built from,
please report it — see [SECURITY.md](SECURITY.md). Serious cases can also be reported
directly to SignPath Foundation, who can revoke the certificate.

## Licence

WinHTTrack is free software under the GNU General Public License v3 or later; see
[COPYING](COPYING). There is no commercial dual-licensing.
