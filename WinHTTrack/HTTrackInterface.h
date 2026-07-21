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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include "httrack-library.h"
/**/
//#include "htsglobal.h"
//#include "htsbase.h"
#include "htsopt.h"
#include "htsdefines.h"
#include "htsstrings.h"
#include "htssafe.h"

extern int linput(FILE* fp,char* s,int max);
extern int linput_trim(FILE* fp,char* s,int max);
extern int linput_cpp(FILE* fp,char* s,int max);
extern void rawlinput(FILE* fp,char* s,int max);
extern int binput(char* buff,char* s,int max);
extern int fexist(const char* s);
extern size_t fsize(const char* s);
extern TStamp time_local(void);

extern char* convtolower(char* catbuff,const char* a);
extern void hts_lowcase(char* s);

extern char* next_token(char* p,int flag);

// Engine internal variables
extern HTSEXT_API hts_stat_struct HTS_STAT;
extern int _DEBUG_HEAD;
extern FILE* ioinfo;

// various
#define copychar(a) concat(catbuff,(a),NULL)
