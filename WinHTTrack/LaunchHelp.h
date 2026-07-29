/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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

#if !defined(LAUNCHHELP_HGDHDGCJHHFIJKCHSOZIOJC5448545245451)
#define LAUNCHHELP_HGDHDGCJHHFIJKCHSOZIOJC5448545245451

#include "stdafx.h"

// Build a browsable file: URL for a doc page. dir is the ANSI directory holding it, page
// is "guide.html" or "guide.html#win/opt-limits". False if the path cannot be encoded.
// Public only for --selftest.
bool BuildDocUrl(const char* dir, const char* page, CString& url);

// Opens the documentation in the system default browser.
class LaunchHelp {
public:
  // The documentation index.
  void Help();
  // A page under html/, optionally with a #fragment.
  void Help(CString page);
  // The guide, at one of its Windows sections, e.g. "opt-limits".
  void HelpTopic(const char* anchor);
};

#endif

