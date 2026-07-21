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
// Classe de sauvegarde de clés (identifiées par leur nom)
// à la manière de la base de registre (mais en plus basique)

#if !defined(MEMREGISTER_LIB_JHGFHIV25489654156HJRZDSCIOUJ5648654651)
#define MEMREGISTER_LIB_JHGFHIV25489654156HJRZDSCIOUJ5648654651

class MemRegister
{
private:
  CStringArray Mem_index;
  CStringArray Mem_value;
  CWordArray Mem_valueint;
public:
  void deleteAll();
  CString getString(CString name,CString defval);
  int getInt(CString name,int defval);
  bool setString(CString name,CString val);
  bool setInt(CString name,int val);
};


#endif

