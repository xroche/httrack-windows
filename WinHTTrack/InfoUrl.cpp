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
// InfoUrl.cpp : implementation file
//

#include "stdafx.h"

/* Externe C */
#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
  #include "htscore.h"
}

#include "Shell.h"
#include "InfoUrl.h"
#include "NewLang.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern HICON httrack_icon;

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl dialog


CInfoUrl::CInfoUrl(CWnd* pParent /*=NULL*/)
	: CDialog(CInfoUrl::IDD, pParent)
{
	//{{AFX_DATA_INIT(CInfoUrl)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CInfoUrl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CInfoUrl)
	DDX_Control(pDX, IDC_backlist, m_ctl_backlist);
	DDX_Control(pDX, IDC_slider, m_slider);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CInfoUrl, CDialog)
	//{{AFX_MSG_MAP(CInfoUrl)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_Freeze, OnFreeze)
	ON_WM_CREATE()
	ON_CBN_SELCHANGE(IDC_backlist, OnSelchangebacklist)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CInfoUrl message handlers

BOOL CInfoUrl::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
  SetIcon(httrack_icon,false);
  SetIcon(httrack_icon,true);  
  SetForegroundWindow();   // yop en premier plan!

  // Patcher l'interface pour les Français ;-)
  if (LANG_T(-1)) {    // Patcher en français
    SetWindowTextCP(this, LANG_X1);
    SetDlgItemTextCP(this, IDC_Freeze,LANG_X2);
    SetDlgItemTextCP(this, IDC_STATIC_moreinfos,LANG_X3);
    SetDlgItemTextCP(this, IDOK,LANG_CLOSE);
  }

  StartTimer();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CInfoUrl::StartTimer() {
  if (!timer) {
    timer=SetTimer(WM_TIMER,100,NULL);
  }
}
void CInfoUrl::StopTimer() {
  if (timer) {
    KillTimer(timer);
    timer=0;
  }
}

const char* ToBool(int i) {
  if (i)
    return "yes";
  else
    return "no";
}

const char* ToStatuscode(int statuscode) {
  if (statuscode<0) {
    return "unknown status";
  } else {
    static char msg[1024];
    msg[0]='\0';
    infostatuscode(msg,statuscode);
    return msg;
  }
}

const char* ToStatus(int i) {
  switch(i) {
  case 0:
    return "ready";
    break;
  case 1:
    return "receiving";
    break;
  case 101:
    return "searching dns";
    break;
  case 100:
    return "waiting for connection";
    break;
  case 99:
    return "receiving header";
    break;
  case 98:
    return "receiving chunk header";
    break;
  case 1000:
    return "ftp session";
    break;
  case -1:
    return "empty";
    break;
  default:
    return "unknown";
    break;
  }
}

void CInfoUrl::OnTimer(UINT_PTR nIDEvent)
{
  static char old_info[8192]="";
  //
  if (termine) {
    EndDialog(IDOK);
    return;
  }
  // Snapshot the engine-owned rows under the lock, then render outside it: reaching
  // into the live back[] here was a use-after-free (unlocked, freed on engine exit).
  // Single modal CInfoUrl at a time, so a static copy is safe and spares the stack.
  static t_StatsBuffer rows[NStatsBuffer];
  WHTT_LOCK();
  if (termine) {
    WHTT_UNLOCK();
    EndDialog(IDOK);
    return;
  }
  memcpy(rows,StatsBuffer,sizeof(rows));
  WHTT_UNLOCK();
  //
  int cur=id;
  if (cur<0 || cur>=NStatsBuffer) cur=0;
  //
  if (m_ctl_backlist.GetDroppedState()==0) {
    m_ctl_backlist.Clear();
    m_ctl_backlist.ResetContent();
    int i;
    for(i=0;i<NStatsBuffer;i++) {
      if (rows[i].etat[0]) {
        char st[HTS_URLMAXSIZE*4];
        sprintf(st,"%02d: ",i);
        strncatbuff(st,rows[i].url_adr,256);
        strncatbuff(st,rows[i].url_fil,256);
        m_ctl_backlist.AddString(st);
      }
    }
  }
  //
  {
    t_StatsBuffer* it=&rows[cur];
    char info[8192]; info[0]='\0';
    char total[256]; total[0]='\0';
    char info100[256]; info100[0]='\0';
    int offset=0;
    if (it->etat[0]) {        // ligne active
      if (it->r_totalsize>0) {
        sprintf(total,LLintP,(LLint)it->r_totalsize);
        offset=(int) ((LONGLONG) ((LONGLONG) it->r_size*(LONGLONG) 100)/((LONGLONG) it->r_totalsize));
        sprintf(info100,"(%d%%)",offset);
      } else {
        sprintf(total,"unknown");
        info100[0]='\0';
      }
      sprintf(info,"File: %s\r\nTotal length: %s\r\nBytes downloaded: " LLintP " %s\r\nCurrent state: %s",it->url_sav,total,it->r_size,info100,ToStatus(it->status));
    }
    //
    if (strcmp(old_info,info)) {
      char moreinfo[8192]; moreinfo[0]='\0';
      if (it->etat[0]) {        // ligne active
        sprintf(moreinfo+strlen(moreinfo),"Host: %s\r\n",it->url_adr);
        sprintf(moreinfo+strlen(moreinfo),"File: %s\r\n",it->url_fil);
        sprintf(moreinfo+strlen(moreinfo),"Name: %s\r\n",it->url_sav);
        if (it->location[0])
          sprintf(moreinfo+strlen(moreinfo),"MoveToLocation: %s\r\n",it->location);
        sprintf(moreinfo+strlen(moreinfo),"ContentType: %s\r\n",it->contenttype);
        sprintf(moreinfo+strlen(moreinfo),"StatusCode: %d (%s)\r\n",it->statuscode,ToStatuscode(it->statuscode));
        sprintf(moreinfo+strlen(moreinfo),"InternalStatus: %d (%s)\r\n",it->status,ToStatus(it->status));
        if (it->msg[0])
          sprintf(moreinfo+strlen(moreinfo),"StatusMessage: %s\r\n",it->msg);
        sprintf(moreinfo+strlen(moreinfo),"HTTP/1.1: %s\r\n",ToBool(it->http11));
        sprintf(moreinfo+strlen(moreinfo),"ChunkMode: %s\r\n",ToBool(it->is_chunk));
        if (it->lien_chunk)
          sprintf(moreinfo+strlen(moreinfo),"CurrentChunkSize: " LLintP "\r\n",it->chunk_size);
        sprintf(moreinfo+strlen(moreinfo),"TestMode: %s\r\n",ToBool(it->testmode));
        sprintf(moreinfo+strlen(moreinfo),"HeadRequest: %s\r\n",ToBool(it->head_request));
        sprintf(moreinfo+strlen(moreinfo),"NotModified: %s\r\n",ToBool(it->notmodified));
        sprintf(moreinfo+strlen(moreinfo),"WriteToDisk: %s\r\n",ToBool(it->is_write));
        sprintf(moreinfo+strlen(moreinfo),"LocalFile: %s\r\n",ToBool(it->is_file));
        sprintf(moreinfo+strlen(moreinfo),"Size: " LLintP "\r\n",it->r_size);
        sprintf(moreinfo+strlen(moreinfo),"TotalSize: " LLintP "\r\n",it->r_totalsize);
      } else
        strcpybuff(moreinfo,"Transfer complete in this buffer, waiting for next file");
      //
      SetDlgItemTextUTF8(this, IDC_InfoUrl,info);
      SetDlgItemTextUTF8(this, IDC_Info100,info100);
      SetDlgItemTextUTF8(this, IDC_MoreInfoUrl,moreinfo);
      //
      m_slider.SetRange(0,100);
      m_slider.SetPos(offset);
      m_slider.RedrawWindow();
      //
      strcpybuff(old_info,info);
    }
  }
  //
  CDialog::OnTimer(nIDEvent);
}

void CInfoUrl::OnClose() 
{
  StopTimer();
	CDialog::OnClose();
}

void CInfoUrl::OnFreeze() 
{
  if (this->IsDlgButtonChecked(IDC_Freeze)) {
    StopTimer();
  } else {
    StartTimer();
  }
}

int CInfoUrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
  timer=0;	

	return 0;
}

void CInfoUrl::OnSelchangebacklist() 
{
  CString st;
  int index=m_ctl_backlist.GetCurSel();
  if (index!=CB_ERR) {
    m_ctl_backlist.GetLBText(index,st);
    int a=st.Find(':');
    if (a>=0) {
      char s[256]; s[0]='\0';
      strncatbuff(s,st,a);
      sscanf(s,"%d",&id);
      this->OnTimer(0);
    }
  }
}
