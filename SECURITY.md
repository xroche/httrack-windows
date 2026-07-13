# Security Policy

This repository holds the Windows GUI (WinHTTrack). The crawler engine lives in
[xroche/httrack](https://github.com/xroche/httrack) and has its own
[security policy](https://github.com/xroche/httrack/blob/master/SECURITY.md); a bug in
the engine is best reported there. When in doubt, report here and we will route it.

## Reporting

Report privately, not in a public issue or PR: use GitHub
[private advisories](https://github.com/xroche/httrack-windows/security/advisories/new)
or email <roche@httrack.com> (alternate: `xroche at gmail dot com`).

Include the WinHTTrack version and the Windows version, a concrete reproduction (the
steps or project settings, a sample page or server response, or a small proof of
concept), and what an attacker gains. We'll acknowledge it and keep you posted. Please
allow time for a release before disclosing publicly.

Distribution problems count: a WinHTTrack installer that is signed but should not have
been, a signed binary that does not match the source it claims to be built from, or a
download that does not match what CI produced. See [SIGNING.md](SIGNING.md) for how
releases are signed and how to verify one.

## Supported versions

Fixes land on `master` and ship in the next release; older releases aren't maintained.
Confirm against current `master` when you can.

## AI-assisted findings

Scanners and LLMs are fine, but only send reports you have verified yourself. A
confirmed, reproducible issue is worth our time; a plausible one that doesn't
reproduce is not, and will be closed. If a report is AI-assisted, say so.
