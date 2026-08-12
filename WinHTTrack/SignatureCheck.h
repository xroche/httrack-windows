/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: WinHTTrack subroutines:                                */
/*       Authenticode check of our own binaries                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINHTTRACK_SIGNATURECHECK_H
#define WINHTTRACK_SIGNATURECHECK_H

#include <windows.h>
#include <stdio.h>

/* Anything short of proof is WHTT_SIG_UNKNOWN. A Windows 7 that never picked up the
   Certum root, or that predates SHA-2 Authenticode, answers that way for our own
   release binaries, so only OTHERS and TAMPERED may ever accuse the user. */
typedef enum {
  WHTT_SIG_UNKNOWN = 0,
  WHTT_SIG_NONE,        /* no signature at all: a build from source */
  WHTT_SIG_OURS,
  WHTT_SIG_OTHERS,      /* valid, and somebody else's certificate */
  WHTT_SIG_TAMPERED     /* signed, over bytes that have changed since */
} WhttSigVerdict;

/* Verify one file, filling in the signer's name when it carries one. Opens no
   connection, so it is safe offline; publisher and status may be NULL. status
   receives the raw WinVerifyTrust result, which is what tells one unverifiable
   system apart from another. */
WhttSigVerdict WhttSigVerifyFile(const char *path, char *publisher,
                                 size_t publisherSize, LONG *status);

/* Verify this program and the engine beside it, on a background thread: chain
   building can block for seconds, and none of this may delay startup. notify, when
   given, is woken once the verdict is in so an idle message pump comes back to it. */
void WhttSigCheckStart(HWND notify);

/* TRUE once the check has finished and it found another publisher, or tampering. */
BOOL WhttSigIsUnofficial(void);

/* One line naming the publisher. Never NULL, empty until the check finishes. */
const char *WhttSigSummary(void);

/* TRUE for the first caller only, once there is something to warn about. */
BOOL WhttSigTakeWarning(void);

/* --check-signature: a verdict line per file, or for path alone when given. */
void WhttSigReport(FILE *out, const char *path);

#endif
