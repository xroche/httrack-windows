/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2014 Xavier Roche and other contributors

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
/*       Crash reporting                                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Initialize crash reporter.
void CrashReportInit(void);

// Report a crash, with msg as additional message, and file:line information
// A stack trace will be generated from the calling point
void CrashReportReport(const char* msg, const char* file, int line);

// Report a crash, with msg as additional message, and file:line information
// also include an optional trace, or no trace if trace is NULL
void CrashReportReportEx(const char* msg, const char* file, int line, const char *trace);
