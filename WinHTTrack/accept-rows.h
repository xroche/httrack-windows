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
/*       the rows of the Accept headers box                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef ACCEPT_ROWS_H
#define ACCEPT_ROWS_H

/* The box has one row for the default Accept and one per kind of link, in the order
   hts_accept_kinds() lists the kinds. ROW takes the row's name, the engine's kind word
   and the winprofile.ini key. The default row carries no kind, so --accept takes its
   value on its own, where a kind row sends "kind=value". */
#define ACCEPT_ROWS(ROW)                                                       \
  ROW(ACCEPT_ROW_DEFAULT, NULL, "AcceptDefault")                               \
  ROW(ACCEPT_ROW_DOCUMENT, "document", "AcceptDocument")                       \
  ROW(ACCEPT_ROW_IFRAME, "iframe", "AcceptIframe")                             \
  ROW(ACCEPT_ROW_FRAME, "frame", "AcceptFrame")                                \
  ROW(ACCEPT_ROW_IMAGE, "image", "AcceptImage")                                \
  ROW(ACCEPT_ROW_STYLE, "style", "AcceptStyle")                                \
  ROW(ACCEPT_ROW_SCRIPT, "script", "AcceptScript")                             \
  ROW(ACCEPT_ROW_FONT, "font", "AcceptFont")                                   \
  ROW(ACCEPT_ROW_AUDIO, "audio", "AcceptAudio")                                \
  ROW(ACCEPT_ROW_VIDEO, "video", "AcceptVideo")                                \
  ROW(ACCEPT_ROW_OBJECT, "object", "AcceptObject")                             \
  ROW(ACCEPT_ROW_EMBED, "embed", "AcceptEmbed")

/* One of the columns as an array, for a caller that wants it. */
#define ACCEPT_ROW_NAME(name, kind, key) name,
#define ACCEPT_ROW_KIND(name, kind, key) kind,

enum { ACCEPT_ROWS(ACCEPT_ROW_NAME) ACCEPT_ROW_COUNT };

/* The longest kind word and its '=' sign ride the same argv word as the value, so
   isAcceptArgument() holds the sum against the engine's ceiling on one argument. */
#define ACCEPT_KIND_PREFIX_MAXSIZE (sizeof("document=") - 1)

#endif
