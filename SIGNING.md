# Code signing policy

WinHTTrack's Windows binaries and its installer carry an Authenticode signature, so that
Windows names a verified publisher instead of warning you about an unknown one.

## Who signs

The certificate is a Certum Open Source Code Signing certificate, issued to Xavier Roche,
who writes HTTrack. Certum prefixes every certificate of that kind, so the publisher
Windows shows reads *Open Source Developer, Xavier Roche*. The private key lives in
Certum's cloud HSM and cannot be exported from it.

| | |
| --- | --- |
| Subject | `CN=Open Source Developer Roche Xavier, L=Paris, C=FR` |
| SHA-1 thumbprint | `AEEE0C1672811FEBB33099FB433FC0D70E5B89C2` |
| Key | RSA 4096, signatures are SHA-256 |
| Timestamps | `http://time.certum.pl`, RFC 3161 |
| Valid | 2026-07-28 to 2027-07-28 |

## What is signed

Every executable file in the package:

| File | What it is |
| --- | --- |
| `WinHTTrack.exe` | the GUI |
| `httrack.exe` | the command-line version |
| `libhttrack.dll` | the engine, built from [xroche/httrack](https://github.com/xroche/httrack) |
| the other DLLs beside it | OpenSSL, zlib and the compression libraries, compiled from source during the build |
| `httrack_*_*.exe` | the Inno Setup installer |
| `unins000.exe` | the uninstaller it leaves behind |

## How signing happens

Only one thing ever signs a WinHTTrack binary: the
[`windows-build`](.github/workflows/windows-build.yml) workflow, on a GitHub-hosted
runner, in the `xroche/httrack-windows` repository, on a version tag or on a run a
maintainer starts by hand. There is no key on a developer's machine to sign with. The signing job runs in a GitHub environment that
requires a human approval before it starts, so pushing a tag does not by itself sign
anything.

Every push and every pull request builds an installer, installs it, runs it and
uninstalls it, unsigned. A tag adds a second job that takes those same binaries, signs
each one, and rebuilds the installer around them. Inno Setup packs those binaries as
opaque bytes, so signing the installer afterwards would leave everything inside it
unsigned. The signed installer is then installed and run in turn, and every signature is
checked against the thumbprint above before it is published.

## Verifying a signature

Right-click the installer, choose *Properties*, and open the *Digital Signatures* tab. Or
from PowerShell:

```powershell
Get-AuthenticodeSignature .\httrack_x64_3.50-beta-1.exe |
  Format-List Status, SignerCertificate, TimeStamperCertificate
```

`Status` must say `Valid`, and the thumbprint must be the one above. The thumbprint is
the part worth checking: any valid certificate at all produces `Valid`. The signatures
carry an RFC 3161 timestamp, so they outlive the certificate that made them.

Source releases of the engine are a separate matter: they are signed with the HTTrack PGP
key `rsa4096/60C3AA7180598EFB`, which has nothing to do with Authenticode and is
documented on [httrack.com](https://www.httrack.com/).

## Privacy policy

This program will not transfer any information to other networked systems unless
specifically requested by the user or the person installing or operating it.

Copying a website is exactly such a request: when you give HTTrack a URL, it contacts that
site, and the sites it links to if you tell it to follow them. It sends nothing anywhere
else, and it reports nothing back to us. HTTrack obeys `robots.txt` by default.

## Reporting a problem

Tell us if you think a WinHTTrack binary was signed that should not have been, or you find
a signed binary that does not match the source it claims to come from. See
[SECURITY.md](SECURITY.md). A serious case can also go straight to Certum, who can revoke
the certificate.

## Licence

WinHTTrack is free software under the GNU General Public License v3 or later; see
[COPYING](COPYING). It is not dual-licensed for commercial use.
