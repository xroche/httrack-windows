/*----------------------------------------------------------------------
Copyright (c) 1998,1999 Gipsysoft. All Rights Reserved.
File:	DynamicRAS.h
Owner:	russf@gipsysoft.com
Purpose:	Dynamically loaded RAS.
----------------------------------------------------------------------*/

/* Thanks to Russ Freeman from gipsymedia */

#include "stdafx.h"
#include "RasLoad.h"

// Absolute System32 path: an unqualified name searches the app directory first, so a
// planted rasapi32.dll would win. (LOAD_LIBRARY_SEARCH_SYSTEM32 needs KB2533623 on Win7.)
static HINSTANCE LoadSystemRas()
{
	TCHAR path[MAX_PATH];
	const UINT len = GetSystemDirectory( path, MAX_PATH );
	static const TCHAR dll[] = _T("\\rasapi32.dll");
	// 0 means failure; len excludes the NUL that _countof(dll) counts
	if ( len == 0 || len >= MAX_PATH - _countof(dll) )
		return NULL;
	_tcscat_s( path, MAX_PATH, dll );
	return LoadLibrary( path );
}

CDynamicRAS::CDynamicRAS()
	: m_hInst( LoadSystemRas() )
	, pRasEnumConnections( NULL )
	, pRasHangUp( NULL )
	, pRasGetConnectStatus( NULL )
  , pRasDial( NULL )
  , pRasEnumEntries( NULL )
  , pRasGetEntryDialParams( NULL )
{
	if( IsRASLoaded() )
	{
		pRasEnumConnections = (PRASENUMCONNECTIONS)GetProcAddress( m_hInst, "RasEnumConnectionsA" );
		pRasHangUp = (PRASHANGUP)GetProcAddress( m_hInst, "RasHangUpA" );
		pRasGetConnectStatus = (PRASGETCONNECTSTATUS)GetProcAddress( m_hInst, "RasGetConnectStatusA" );
		pRasDial = (PRASDIAL)GetProcAddress( m_hInst, "RasDialA" );
    pRasEnumEntries = (PRASENUMENTRIES)GetProcAddress( m_hInst, "RasEnumEntriesA" );
    pRasGetEntryDialParams = (PRASGETENTRYDIALPARAMS)GetProcAddress( m_hInst, "RasGetEntryDialParamsA" );
	}
}

CDynamicRAS::~CDynamicRAS()
{
	if( IsRASLoaded() )
	{
		FreeLibrary( m_hInst );
	}
}



