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
/*       GUI version, distinct from the engine's                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WINHTTRACK_VERSION_H
#define WINHTTRACK_VERSION_H

/* The GUI versions itself, independently of the engine. Bump it here only: the .rc,
   the installer and CI all read this file, and the release tag must match it. */
#define WINHTTRACK_VERSION "3.50"

/* Dotted, for Inno and the FileVersion field. The betas sat below this, at 3.49.99.N. */
#define WINHTTRACK_VERSIONID "3.50.0.0"

/* WINHTTRACK_VERSIONID in the resource compiler's comma form. */
#define WINHTTRACK_VERSION_NUM 3, 50, 0, 0

#endif
