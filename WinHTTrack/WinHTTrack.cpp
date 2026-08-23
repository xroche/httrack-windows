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
// WinHTTrack.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "WinHTTrack.h"

#include "Shell.h"

#include "wid1.h"
#include "maintab.h"

#include "MainFrm.h"
#include "splitter.h"
#include "about.h"

#include "WinHTTrackDoc.h"
#include "WinHTTrackView.h"

#include "inprogress.h"

#include "CrashReport.h"
#include "SignatureCheck.h"
#include "Unofficial.h"
#include "version.h"

// KB955045 (http://support.microsoft.com/kb/955045)
// To execute an application using this function on earlier versions of Windows
// (Windows 2000, Windows NT, and Windows Me/98/95), then it is mandatary to #include Ws2tcpip.h
// and also Wspiapi.h. When the Wspiapi.h header file is included, the 'getaddrinfo' function is
// #defined to the 'WspiapiGetAddrInfo' inline function in Wspiapi.h. 
#include <ws2tcpip.h>
#include <Wspiapi.h>
#ifndef getaddrinfo
#error getaddrinfo "should be defined"
#define getaddrinfo WspiapiGetAddrInfo
#endif

/* HTS - HTTRACK */
extern "C" {
  #include "HTTrackInterface.h"
  //#include "htsbase.h"
  //#include "htsglobal.h"
  //#include "htsthread.h"
};
#include <Ws2tcpip.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
#include "DialogContainer.h"
#include "InfoUrl.h"

// No DoModal() outside the main thread!
#include "wizard.h"
#include "wizard2.h"
#include "WizLinks.h"
#include "XSHBrowseForFolder.h"


extern Wid1* dialog1;
extern CMainTab* maintab;
extern Cinprogress* inprogress;
extern CShellApp* CShellApp_app;
extern int termine;
extern int termine_requested;
extern int soft_term_requested;
extern int shell_terminated;
extern CInfoUrl* _Cinprogress_inst;
extern int LibRasUse;

/*extern "C" {
  char* hts_rootdir(char* file);
};*/


// rmdir
#include <direct.h>

// linput
/*extern "C" {
  void linput(FILE* fp,char* s,int max);
  void linput_trim(FILE* fp,char* s,int max);
  void linput_cpp(FILE* fp,char* s,int max);
};*/

/* WinHTTrack refresh Mutex */
HANDLE WhttMutex;

/* Location */
const char* WhttLocation="";


// HTTrack main vars
HWND App_Main_HWND;
CSplitterFrame* this_CSplitterFrame=NULL;
HICON httrack_icon;
// Helper
LaunchHelp* HtsHelper=NULL;

// dirtreeview
#include "DirTreeView.h"
extern CDirTreeView* this_DirTreeView;

// New Project
#include "NewProj.h"
extern CNewProj* dialog0;


// InfoEnd
#include "infoend.h"
extern Cinfoend* this_Cinfoend;

// Pointeur sur nous
CWinHTTrackApp* this_app=NULL;

// fexist
extern "C" int fexist(const char*);

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp

BEGIN_MESSAGE_MAP(CWinHTTrackApp, CWinApp)
	//{{AFX_MSG_MAP(CWinHTTrackApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_FILE_MRU_FILE1, OnFileMruFile1)
	//}}AFX_MSG_MAP
  ON_COMMAND(wm_ViewRestart,OnViewRestart)
  ON_COMMAND(wm_WizRequest1,OnWizRequest1)
  ON_COMMAND(wm_WizRequest2,OnWizRequest2)
  ON_COMMAND(wm_WizRequest3,OnWizRequest3)
	// Standard file based document commands
	//ON_COMMAND(ID_FILE_WIZARD, OnWizard)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_DELETE_PROJ, OnFileDelete)
	ON_COMMAND(ID_FILE_BROWSE_SIT, OnBrowseWebsites)
  ON_COMMAND(IDC_langprefs,Onipabout)
  ON_COMMAND(ID_ABOUT,Onipabout)
  ON_COMMAND(ID_UPDATE,OnUpdate)
	ON_COMMAND(ID_HELP_FINDER,OnHelpInfo2)
	ON_COMMAND(ID_HELP,OnHelpInfo2)
	//ON_COMMAND(ID_CONTEXT_HELP,OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP,OnHelpInfo2)
  // Forward to inprogress
  ON_BN_CLICKED(ID_LOAD_OPTIONS,FwOnLoadprofile)
  ON_BN_CLICKED(ID_FILE_SAVE_OPTIONS_AS,FwOnSaveprofile)
	ON_BN_CLICKED(ID_LoadDefaultOptions, FwOnLoaddefault)
	ON_BN_CLICKED(ID_SaveDefaultOptions, FwOnSavedefault)
	ON_BN_CLICKED(ID_ClearDefaultOptions,FwOnResetdefault)
  //
  ON_BN_CLICKED(ID_WINDOW_HIDE,FwOnhide)
  //
	ON_BN_CLICKED(ID_OPTIONS_MODIFY,FwOnModifyOpt)
	ON_BN_CLICKED(ID_FILE_PAUSE,FwOnPause)
	ON_BN_CLICKED(ID_LOG_VIEWLOG,FwOniplogLog)
	ON_BN_CLICKED(ID_LOG_VIEWERRORLOG,FwOniplogErr)
	ON_BN_CLICKED(ID_LOG_VIEWTRANSFERS,FwOnViewTransfers)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp construction

CWinHTTrackApp::CWinHTTrackApp()
{
  // HTTrack inits
  CreateMutex(NULL, FALSE, "WinHTTrack_RUN");
  HtsHelper = new LaunchHelp();
}

CWinHTTrackApp::~CWinHTTrackApp()
{
  DeleteTabs();
  delete HtsHelper;
  HtsHelper=NULL;
	if (global_opt != NULL)
	{
		hts_free_opt(global_opt);
		global_opt = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWinHTTrackApp object

CWinHTTrackApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp initialization

int Eval_Exception( void );

int Eval_Exception ( int n_except )
{
    AfxMessageBox("error");

    return 0;
}

#include "mmsystem.h"

static void httrackErrorCallback(const char* msg, const char* file, int line) {
  CrashReportReport(msg, file, line);
}

/* Set by --selftest. Startup failures must then report on stderr and exit non-zero
   rather than raise a message box: nobody is there to click it, and a modal dialog
   would hang a headless run instead of failing it. */
int WhttSelfTest = 0;

/* A GUI-subsystem process has no console of its own, so borrow the caller's -- but
   only when stdout is not already going somewhere. If it has been redirected to a
   pipe or a file that handle is valid, and reopening CONOUT$ would throw the output
   away. */
void WhttEnsureConsole(void) {
  const HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hout == NULL || hout == INVALID_HANDLE_VALUE) {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      FILE *out = NULL;
      freopen_s(&out, "CONOUT$", "w", stdout);
      freopen_s(&out, "CONOUT$", "w", stderr);
    }
  }
}

/* The update URL, shared with --selftest so what goes on the wire is what a test reads.
   LANGUAGE_FILE, not LANGUAGE_NAME: the basename is ASCII and is what this parameter
   carried before 3.49.23 turned LANGUAGE_NAME into endonyms. */
static CString UpdateUrl() {
  CString st;
  st.Format(HTS_UPDATE_WEBSITE, 0, LANGUAGE_FILE);
  return st;
}

BOOL CWinHTTrackApp::InitInstance()
{
  /* Answer --version without bringing up the UI, so a smoke test can prove the
     binary actually starts. ExitProcess rather than returning FALSE: MFC would
     still run ExitInstance(), which calls hts_uninit() on an engine we never
     started. Nothing is initialised yet, so there is nothing to unwind. */
  /* __argv is only populated in an MBCS build, which this is; guard anyway so a
     future Unicode switch cannot turn this into a null dereference. */
  for (int i = 1; __argv != NULL && i < __argc; i++) {
    if (strcmp(__argv[i], "--version") == 0) {
      WhttEnsureConsole();
      printf("WinHTTrack %s (engine %s)\n", WINHTTRACK_VERSION, HTTRACK_VERSION);
      fflush(stdout);
      ExitProcess(0);
    } else if (strcmp(__argv[i], "--check-signature") == 0) {
      /* Who signed what, with the raw status: a support answer, and what proves in CI
         that a modified binary really is noticed. */
      WhttEnsureConsole();
      WhttSigReport(stdout, (i + 1 < __argc) ? __argv[i + 1] : NULL);
      ExitProcess(0);
    } else if (strcmp(__argv[i], "--selftest") == 0) {
      /* Carry on through the real startup and report from there. --version only
         proves the binary loads; it says nothing about whether the installation
         is complete, which is what actually broke. */
      WhttSelfTest = 1;
      WhttEnsureConsole();
    }
  }

  /* See <https://msdn.microsoft.com/library/ff919712> */
#if (defined(_WIN32) && (!defined(_DEBUG)))
  {
    /* Narrow the search to the application directory and System32; it matters most to
       the ZIP package, which people unpack into Downloads. Resolved and not imported: a
       Windows 7 without KB2533623 has no such export and would fail to load at all. */
#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#endif
    BOOL (WINAPI*const k32_SetDefaultDllDirectories)(DWORD) =
      (BOOL (WINAPI *)(DWORD))
      GetProcAddress(GetModuleHandle("kernel32.dll"), "SetDefaultDllDirectories");
    if (k32_SetDefaultDllDirectories != NULL) {
      k32_SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    }

    /* Still worth doing where the above is missing, and harmless where it is not.
    See KB 2389418
    "If this parameter is an empty string (""), the call removes the
    current directory from the default DLL search order" */
    BOOL (WINAPI*const k32_SetDllDirectoryA)(LPCSTR) = 
      (BOOL (WINAPI *)(LPCSTR))
      GetProcAddress(GetModuleHandle("kernel32.dll"), "SetDllDirectoryA");
    /* The old GetVersion() gate only existed to tolerate pre-Windows-2000, which
       the Windows 7 floor rules out; the call is deprecated, so drop it. */
    if (k32_SetDllDirectoryA != NULL && !k32_SetDllDirectoryA("")) {
      assertf(!"SetDllDirectory failed");
    }
  }
#endif

  /* No error messageboxes */
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX);

  /* Inits */
  CrashReportInit();
  hts_set_error_callback(httrackErrorCallback);
  hts_init();

  WhttMutex = CreateMutex(NULL,FALSE,NULL);

  // Change the registry key under which our settings are stored.
  // TODO: You should modify this string to be something appropriate
  // such as the name of your company or organization.
  SetRegistryKey("WinHTTrack Website Copier");
  LANG_INIT();    // petite init langue

  /* --selftest: everything that depends on the installed data files has now run
     (lang.def above all). Report and leave before any window appears. */
  if (WhttSelfTest) {
    // Only reachable by typing into a dialog, so test the MBCS->UTF-8 conversion here instead.
    {
      static const WCHAR wide[] = { 'c', 'a', 'f', 0x00E9, 0 };   /* cafe-acute */
      BOOL lost = FALSE;
      // WideCharToMultiByte rejects a non-NULL lpUsedDefaultChar when the code page is CP_UTF8.
      BOOL *const plost = (GetACP() == CP_UTF8) ? NULL : &lost;
      char ansi[16];
      const int n = WideCharToMultiByte(CP_ACP, 0, wide, -1, ansi, sizeof(ansi),
                                        NULL, plost);
      if (n > 0 && !lost) {   /* skip where the ANSI codepage cannot hold it at all */
        int nchecks = 0;
        char *got = strdupt_utf8(ansi);   /* freet() nulls it, so not const */
        if (got == NULL || strcmp(got, "caf\xc3\xa9") != 0) {
          fprintf(stderr, "FATAL: MBCS->UTF-8 gave '%s', expected caf\xc3\xa9\n",
                  got != NULL ? got : "(null)");
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
        freet(got);
        /* The install path is where a non-ASCII byte reaches the help URL. */
        {
          char dir[64];
          CString url;
          sprintfbuff(dir, "C:\\%s\\html\\", ansi);
          if (!BuildDocUrl(dir, "guide.html", url) || url != "file:///C:/caf%C3%A9/html/guide.html") {
            fprintf(stderr, "FATAL: non-ASCII help path gave '%s'\n", (LPCSTR) url);
            fflush(stderr);
            ExitProcess(3);
          } else
            nchecks++;
        }
        /* Printed after both checks, so neither can be dropped unnoticed. */
        printf("MBCS->UTF-8 ok on %d checks\n", nchecks);
      }
    }
    /* An unescaped space truncates the path, and an unescaped '#' names a file rather
       than a section, since NTFS allows one in a filename. */
    {
      static const struct { const char* dir; const char* page; const char* want; } urls[] = {
        { "C:\\Program Files\\WinHTTrack\\html\\", "guide.html#win/opt-limits",
          "file:///C:/Program%20Files/WinHTTrack/html/guide.html#win/opt-limits" },
        { "C:\\h\\", "guide.html", "file:///C:/h/guide.html" },
        { "C:\\a#b\\", "guide.html#win", "file:///C:/a%23b/guide.html#win" },
        /* %2C, not %2c: the accented case below pins the hex case too, but it skips
           where the ANSI codepage cannot hold the accent. */
        { "C:\\a,b\\", "guide.html", "file:///C:/a%2Cb/guide.html" },
        { "\\\\srv\\share\\html\\", "guide.html#win", "file://srv/share/html/guide.html#win" },
        { NULL, NULL, NULL }
      };
      /* Counted in the else of each check, so deleting a check takes its count with it. */
      int nchecks = 0;
      for(int k=0 ; urls[k].dir != NULL ; k++) {
        CString url;
        if (!BuildDocUrl(urls[k].dir, urls[k].page, url) || url != urls[k].want) {
          fprintf(stderr, "FATAL: help URL for '%s%s' is '%s', expected '%s'\n",
                  urls[k].dir, urls[k].page, (LPCSTR) url, urls[k].want);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("help URLs ok on %d checks\n", nchecks);
    }
    /* Only reachable by typing into the Experts page, so pin the rule splitter here:
       a rule the engine cannot parse aborts the whole mirror. */
    {
      static const struct { const char* field; const char* want; } rules[] = {
        { "a=b\r\nc=d", "a=b|c=d" },
        { "a=b  c=d", "a=b|c=d" },
        { "a=b\tc=d", "a=b|c=d" },
        { "a=b\vc=d", "a=b|c=d" },
        { "a=b\fc=d", "a=b|c=d" },
        { "a=b \r\n\t c=d", "a=b|c=d" },
        { "  \r\n a=b \r\n  ", "a=b" },
        { "a.com  ,  b.com  =  c.com", "a.com,b.com=c.com" },
        /* the separator ending a line glues it to the next, wherever the rule sits */
        { "a.com,\r\nb.com=c.com", "a.com,b.com=c.com" },
        { "x=y\r\na.com , b.com = c.com", "x=y|a.com,b.com=c.com" },
        /* an empty field must not emit --host-alias "", which the engine refuses */
        { " \r\n\t\v\f ", "" },
        { "", "" },
        { NULL, NULL }
      };
      int nchecks = 0;
      for(int k=0 ; rules[k].field != NULL ; k++) {
        CStringArray got;
        CString joined;
        splitRulesInArray(got, rules[k].field);
        for(INT_PTR j=0 ; j<got.GetSize() ; j++) {
          if (j != 0)
            joined += "|";
          joined += got[j];
        }
        if (joined != rules[k].want) {
          fprintf(stderr, "FATAL: rule field '%s' split into '%s', expected '%s'\n",
                  rules[k].field, (LPCSTR) joined, rules[k].want);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("rule splitting ok on %d checks\n", nchecks);
    }
    /* winprofile.ini escaping: WebHTTrack writes the same file, so both what we
       emit and what we accept are a cross-front-end contract. */
    {
      static const struct { const char* in; const char* want; } vals[] = {
        { "%%", "%" }, { "%3d", "=" }, { "%0d", "\r" }, { "%0a", "\n" }, { "%09", "\t" },
        /* today's decoder turns every other escape into a space, uppercase hex included */
        { "%3D", " " }, { "%41", " " },
        { "a%%b%3dc", "a%b=c" },
        { NULL, NULL }
      };
      /* the coded form is what the other reader sees, so pin it and not just the round trip */
      static const struct { const char* plain; const char* coded; } pairs[] = {
        { "a=b\r\n\tc%d", "a%3db%0d%0a%09c%%d" },
        { "%", "%%" },
        { "100% =", "100%% %3d" },
        { "", "" },
        { NULL, NULL }
      };
      /* Each block counts separately, so dropping one shows up as a zero. */
      int ndecodes = 0, nroundtrips = 0, nterminators = 0;
      for(int k=0 ; vals[k].in != NULL ; k++) {
        if (profile_decode(vals[k].in) != vals[k].want) {
          fprintf(stderr, "FATAL: profile value '%s' decoded to '%s'\n",
                  vals[k].in, (LPCSTR) profile_decode(vals[k].in));
          fflush(stderr);
          ExitProcess(3);
        } else
          ndecodes++;
      }
      for(int k=0 ; pairs[k].plain != NULL ; k++) {
        const CString coded = profile_code(pairs[k].plain);
        if (coded != pairs[k].coded || profile_decode(coded) != pairs[k].plain) {
          fprintf(stderr, "FATAL: profile value '%s' coded to '%s', expected '%s', decoded back to '%s'\n",
                  pairs[k].plain, (LPCSTR) coded, pairs[k].coded, (LPCSTR) profile_decode(coded));
          fflush(stderr);
          ExitProcess(3);
        } else
          nroundtrips++;
      }
      /* the bytes past the NUL are poison: decoding must stop at the terminator */
      {
        static const char truncated[2][8] = { "x%\0zzzz", "x%0\0zzz" };

        for(size_t k=0 ; k<sizeof(truncated)/sizeof(truncated[0]) ; k++) {
          const CString got = profile_decode(truncated[k]);

          if (got != "x ") {
            fprintf(stderr, "FATAL: truncated escape '%s' decoded to '%s'\n",
                    truncated[k], (LPCSTR) got);
            fflush(stderr);
            ExitProcess(3);
          } else
            nterminators++;
        }
      }
      printf("profile escaping ok on %d decodes, %d round trips and %d terminators\n",
             ndecodes, nroundtrips, nterminators);
    }
    /* lance() is out of reach here, so this is the only pin on the gating
       contract declared in Shell.h. */
    {
      static const struct {
        const char *warc, *warccdx, *wacz, *sitemap, *sitemapurl,
                   *singlefile, *singlefilemax;
        const char *want;
      } gated[] = {
        /* every box clear with its field filled: the state the front ends read apart */
        { "", "1", "1", "", "http://e/sitemap.xml", "", "1000", "" },
        { "", "1", "1", "", "", "", "", "" },
        { "", "", "", "", "http://e/sitemap.xml", "", "", "" },
        { "", "", "", "", "", "", "1000", "" },
        /* ticked, so the very same fields must come through: else the rows above pass vacuously */
        { "1", "1", "1", "", "", "", "", "--warc|--warc-cdx|--wacz" },
        { "1", "", "", "", "", "", "", "--warc" },
        { "", "", "", "1", "http://e/sitemap.xml", "", "", "--sitemap-url|http://e/sitemap.xml" },
        /* a blank address falls back to the probe, never --sitemap-url "" */
        { "", "", "", "1", " \t ", "", "", "--sitemap" },
        { "", "", "", "", "", "1", "1000", "--single-file|--single-file-max-size|1000" },
        /* a cap that fails validation is dropped, and the box still emits --single-file */
        { "", "", "", "", "", "1", "abc", "--single-file" },
        { "", "", "", "", "", "1", "0", "--single-file" },
        /* in range despite its length, so length alone must not drop it */
        { "", "", "", "", "", "1", "0000000000000000000001",
          "--single-file|--single-file-max-size|0000000000000000000001" },
        /* one sub-option only, so swapping the two branch bodies cannot pass */
        { "1", "1", "", "", "", "", "", "--warc|--warc-cdx" },
        /* all three groups at once: pins their order, and that they are not exclusive */
        { "1", "", "", "1", "http://e/sitemap.xml", "1", "1000",
          "--warc|--sitemap-url|http://e/sitemap.xml|--single-file|--single-file-max-size|1000" },
        { "", "", "", "", "", "1", " 1000 ", "--single-file|--single-file-max-size|1000" },
        /* a leading dash reads to the engine as the argument being missing */
        { "", "", "", "1", "-http://e/", "", "", "--sitemap" },
        { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
      };
      int nchecks = 0;
      for(int k=0 ; gated[k].want != NULL ; k++) {
        CShellOptions opt;
        CSimpleArray<CString> got;
        CString joined;

        opt.warc = gated[k].warc;
        opt.warccdx = gated[k].warccdx;
        opt.wacz = gated[k].wacz;
        opt.sitemap = gated[k].sitemap;
        opt.sitemapurl = gated[k].sitemapurl;
        opt.singlefile = gated[k].singlefile;
        opt.singlefilemax = gated[k].singlefilemax;
        addGatedOptions(got, opt);
        for(int j=0 ; j<got.GetSize() ; j++) {
          if (j != 0)
            joined += "|";
          joined += got[j];
        }
        if (joined != gated[k].want) {
          fprintf(stderr, "FATAL: gated row %d [%s,%s,%s,%s,%s,%s,%s] emitted '%s', expected '%s'\n",
                  k, gated[k].warc, gated[k].warccdx, gated[k].wacz, gated[k].sitemap,
                  gated[k].sitemapurl, gated[k].singlefile, gated[k].singlefilemax,
                  (LPCSTR) joined, gated[k].want);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("option gating ok on %d checks\n", nchecks);
    }
    /* Pins the engine's grammar through the DLL, so a change to it lands here and
       not in a mirror. */
    {
      static const struct { const char* rule; BOOL want; } aliases[] = {
        { "b.com = a.com", TRUE },       { "*://b.com=a.com", TRUE },
        { "b.com=[::1]", TRUE },         { "b.com=[::1]:8080", TRUE },
        { "b.com//=a.com//", TRUE },     { "b.com,c.com=a.com", TRUE },
        { "*.b.com=a.com:8080", TRUE },
        { "https://www.foo.com=ftp://ftp.foo.com", TRUE },
        { "https://www.foo.com/=ftp://ftp.foo.com/", TRUE },
        { "b.com=https://", FALSE },     { "b.com=///", FALSE },
        { "b.com=primary", FALSE },      { "a.com=user:pw@a.com", FALSE },
        { "b.com", FALSE },              { "=a.com", FALSE },
        { "b.com=", FALSE },             { "b.com=a.com,z.com", FALSE },
        { "b.com=*.a.com", FALSE },      { "b.com=a com", FALSE },
        { "b.com=a.com#x", FALSE },      { "b.com=http://a.com/x", FALSE },
        { "https://www.foo.com/=ftp://ftp.foo.com/pub/", FALSE },
        { "https://www.foo.com/a=ftp://ftp.foo.com", FALSE },
        { "a.com,b.com/deep=c.com", FALSE },
        /* an unknown scheme and a control byte both name no host */
        { "b.com=x://a.com", FALSE },    { "b.com=a.com://c.com", FALSE },
        { "b.com=a\vcom", FALSE },       { "b.com=a\fcom", FALSE },
        { "b.com=a.com\x7f", FALSE },    { "b\v.com=a.com", FALSE },
        { "b.com=a.com\nc.com", FALSE }, { "b.com=ftp://a.com", TRUE },
        /* an accented host reaches the engine as UTF-8 high bytes, which it passes through */
        { "b.com=caf\xE9.example", TRUE },
        /* the engine takes an alias starting with a dash (#1179) */
        { "-legacy.example.com=example.com", TRUE },
        { NULL, FALSE }
      };
      int nchecks = 0;
      for(int k=0 ; aliases[k].rule != NULL ; k++) {
        if (isHostAliasArgument(aliases[k].rule) != aliases[k].want) {
          fprintf(stderr, "FATAL: host-alias rule '%s' judged %s\n",
                  aliases[k].rule, aliases[k].want ? "bad, expected good"
                                                   : "good, expected bad");
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("host-alias rules ok on %d checks\n", nchecks);
    }
    /* The cap rides argv as its own token, so its length costs the mirror as surely as its
       value: the engine refuses an argument of HTS_CDLMAXSIZE bytes before parsing it. */
    {
      static const struct { const char *lead; int repeat; const char *tail; BOOL want; } caps[] = {
        { "", 0, "1000", TRUE },
        { "", 0, "+1000", FALSE },   /* strtoll would take the sign, the engine will not */
        { "", 0, "3000000000", TRUE },   /* over 2 GB, so a 32-bit parse cannot pass this */
        { "9", 19, "", FALSE },   /* overflows LLint at a length the rows below still allow */
        { "0", 4, "", FALSE },   /* zero, however it is written */
        /* leading zeros keep the value at 1, so only the length decides these two */
        { "0", HTS_CDLMAXSIZE - 2, "1", TRUE },
        { "0", HTS_CDLMAXSIZE - 1, "1", FALSE },
        { NULL, 0, NULL, FALSE }
      };
      int nchecks = 0;
      for(int k=0 ; caps[k].lead != NULL ; k++) {
        CString value;

        for(int n=0 ; n<caps[k].repeat ; n++)
          value += caps[k].lead;
        value += caps[k].tail;
        if (isSingleFileMaxArgument(value) != caps[k].want) {
          fprintf(stderr, "FATAL: single-file cap '%s' (%d chars) judged %s\n",
                  (LPCSTR) value.Left(40), (int) value.GetLength(),
                  caps[k].want ? "bad, expected good" : "good, expected bad");
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("single-file caps ok on %d checks\n", nchecks);
    }
    /* Pin what the shell agrees to hand the options it quotes, per option: a value the engine
       refuses costs the whole mirror, and each field carries its own cap. */
    {
      static const struct { BOOL (*ok)(const CString &); const char* field;
                            const char* value; int repeat; BOOL want; } quoted[] = {
        { isFooterArgument, "footer", HTS_DEFAULT_FOOTER, 1, TRUE },
        { isFooterArgument, "footer", HTS_NOPARAM, 1, TRUE },   /* asks for no footer at all */
        { isFooterArgument, "footer", "", 1, FALSE },
        { isFooterArgument, "footer", "-<!-- x -->", 1, FALSE },  /* reads as the argument being missing */
        { isFooterArgument, "footer", "\"<!-- x -->\"", 1, TRUE },   /* the quote is the engine's to escape */
        { isFooterArgument, "footer", "x", HTS_FOOTER_MAXSIZE - 1, TRUE },  /* one under the cap fits */
        { isFooterArgument, "footer", "x", HTS_FOOTER_MAXSIZE, FALSE },
        { isLangIsoArgument, "accept-language", "en, fr", 1, TRUE },
        { isLangIsoArgument, "accept-language", "x", HTS_LANGISO_MAXSIZE - 1, TRUE },
        { isLangIsoArgument, "accept-language", "x", HTS_LANGISO_MAXSIZE, FALSE },
        { isRefererArgument, "referer", "x", HTS_REFERER_MAXSIZE - 1, TRUE },
        { isRefererArgument, "referer", "x", HTS_REFERER_MAXSIZE, FALSE },
        /* the user-agent has no cap of its own, only the argument one the quotes eat into */
        { isUserAgentArgument, "user-agent", "x", HTS_CDLMAXSIZE - 3, TRUE },
        { isUserAgentArgument, "user-agent", "x", HTS_CDLMAXSIZE - 2, FALSE },
        { NULL, NULL, NULL, 0, FALSE }
      };
      int nchecks = 0;
      for(int k=0 ; quoted[k].field != NULL ; k++) {
        CString value;
        for(int n=0 ; n<quoted[k].repeat ; n++)
          value += quoted[k].value;
        if (quoted[k].ok(value) != quoted[k].want) {
          fprintf(stderr, "FATAL: %s argument '%s' (%d chars) judged %s\n",
                  quoted[k].field, (LPCSTR) value.Left(40), (int) value.GetLength(),
                  quoted[k].want ? "bad, expected good" : "good, expected bad");
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      /* The caps count the UTF-8 bytes the engine will see: 200 accented characters are 400 of
         them. Under a UTF-8 ANSI codepage the conversion is a copy, and 200 stay 200. */
      const BOOL mbcs = GetACP() != CP_UTF8;
      if (mbcs) {
        if (isFooterArgument(CString('\xE9', 200))) {
          fprintf(stderr, "FATAL: a 400-byte accented footer was judged short enough\n");
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      /* Count before the suffix: the CI guard rejects the suffix right after it, so a skip
         is still visible. */
      printf("quoted arguments ok on %d checks%s\n", nchecks,
             mbcs ? "" : " (accented case skipped)");
    }
    /* The wizard's answer only reaches the engine through a modal dialog, and a
       wrong number there quietly applies the wrong filter rather than failing. */
    {
      static const struct { int lnk; int scope; const char* want; } answers[] = {
        { 0, -1, "0" }, { 1, -1, "1" }, { 2, -1, "2" },
        { 3, -1, "4" }, { 4, -1, "5" }, { 5, -1, "6" },   /* the engine has no 3 */
        /* a populated combo leaves scope at 0, so the plain rows must ignore it */
        { 0, 0, "0" },  { 1, 0, "1" },  { 2, 0, "2" },
        { 3, 0, "4" },  { 4, 0, "5" },  { 5, 0, "6" },
        { 6, 0, "2000" }, { 6, 12, "2012" },              /* drop the k-th scope */
        { 7, 0, "1000" }, { 7, 12, "1012" },              /* take it */
        { 6, -1, "" },  { 7, -1, "" },                    /* empty scope menu */
        { -1, -1, "" }, { -1, 0, "" }, { 8, 0, "" },      /* nothing selected */
        { 0, 0, NULL }
      };
      int nchecks = 0;
      for(int k=0 ; answers[k].want != NULL ; k++) {
        char got[16] = "unset";
        const bool ok = WizLinkAnswer(answers[k].lnk, answers[k].scope, got, sizeof(got));
        if (ok != (answers[k].want[0] != '\0') || strcmp(got, answers[k].want) != 0) {
          fprintf(stderr, "FATAL: wizard answer for row %d scope %d is '%s', expected '%s'\n",
                  answers[k].lnk, answers[k].scope, got, answers[k].want);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      {   /* "1012" cut to "10" would name a different scope, so it must fail */
        char tiny[3];
        if (WizLinkAnswer(7, 12, tiny, sizeof(tiny)) || tiny[0] != '\0') {
          fprintf(stderr, "FATAL: wizard answer truncated to '%s' instead of failing\n", tiny);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      printf("wizard answers ok on %d checks\n", nchecks);
    }
    /* The answer is an index into this menu, so it must be the engine's list in the
       engine's order. Same cases as the engine's 292_engine-wizard-scope.test. */
    {
      CStringArray scopes;
      int nchecks = 0;

      if (WizHostScopes("download.example.co.uk/x", scopes) != 3
          || scopes[0] != "download.example.co.uk" || scopes[1] != "example.co.uk"
          || scopes[2] != "co.uk") {
        fprintf(stderr, "FATAL: wizard scopes gave %d entries, first '%s'\n",
                (int) scopes.GetSize(),
                scopes.GetSize() != 0 ? (LPCSTR) scopes[0] : "(none)");
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      scopes.RemoveAll();
      if (WizHostScopes("192.168.1.1/x", scopes) != 0) {   /* an IP literal has none */
        fprintf(stderr, "FATAL: wizard offered %d scopes for an IP literal\n",
                (int) scopes.GetSize());
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      printf("wizard scopes ok on %d checks\n", nchecks);
    }
    /* The New Folder button is one restored block away from being live again, and these
       two decisions are the whole of what keeps its copies inside MAX_PATH. */
    {
      static const struct { const char* tail; int repeat; BOOL fits; BOOL sep; } names[] = {
        { "", 0, TRUE, FALSE },                 /* nothing to make a prefix of */
        { "", 1, TRUE, TRUE },
        { "\\", 1, TRUE, FALSE },               /* already a prefix */
        { "", MAX_PATH - 2, TRUE, TRUE },       /* the widest name that can still take one */
        { "", MAX_PATH - 1, TRUE, FALSE },      /* copyable, one byte short of taking one */
        { "\\", MAX_PATH - 2, TRUE, FALSE },    /* the same width, but needing no separator */
        { "", MAX_PATH, FALSE, FALSE },         /* one over the buffer */
        { NULL, 0, FALSE, FALSE }
      };
      int nchecks = 0;
      for(int k=0 ; names[k].tail != NULL ; k++) {
        CString name('x', names[k].repeat);
        bool fits, sep;

        name += names[k].tail;
        fits = XSHBFF_NameOk(name, XSHBFF_FitsPath);
        sep = XSHBFF_NameOk(name, XSHBFF_NeedsSeparator);
        if (fits != (names[k].fits != 0) || sep != (names[k].sep != 0)) {
          fprintf(stderr, "FATAL: a folder name of %d chars fits=%d needs-separator=%d, expected %d and %d\n",
                  (int) name.GetLength(), (int) fits, (int) sep, (int) names[k].fits, (int) names[k].sep);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
      }
      /* Instantiated so the compiler checks it: it is the only correct way to send SETSTRING,
         and nothing calls it while the dialog stays unreachable. */
      static char probe[MAX_PATH];
      XSHBFF_SetDirectReturn(NULL, probe);
      printf("folder names ok on %d checks\n", nchecks);
    }
    /* Only reachable by opening the Browser ID page, so pin the presets here: a
       stray %s or a misspelt {field} would reach every mirrored page. */
    {
      /* Fields counted apart: HTS_DEFAULT_FOOTER is the engine's, so how many it names
         is not ours to pin. */
      int nchecks = 0, nfields = 0;

      /* "{}", "{{", an unterminated "{" and an over-long name all reach the engine as "" */
      if (hts_footer_field_ok("")) {
        fprintf(stderr, "FATAL: the engine expands an empty footer field name\n");
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      if (strcmp(FooterPresets[0], HTS_DEFAULT_FOOTER) != 0) {
        fprintf(stderr, "FATAL: first footer preset is '%s', expected the engine default '%s'\n",
                FooterPresets[0], HTS_DEFAULT_FOOTER);
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      for(int k=0 ; FooterPresets[k] != NULL ; k++) {
        const char* p = FooterPresets[k];
        if (strstr(p, "%s") != NULL) {
          fprintf(stderr, "FATAL: footer preset '%s' uses the legacy %%s model\n", p);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
        if (!isFooterArgument(p)) {
          fprintf(stderr, "FATAL: footer preset '%s' is one the shell would drop\n", p);
          fflush(stderr);
          ExitProcess(3);
        } else
          nchecks++;
        /* An unterminated comment, or one closed early, swallows the page. */
        if (strcmp(p, HTS_NOPARAM) != 0) {
          const char *const tail = strstr(p, "-->");
          if (strncmp(p, "<!--", 4) != 0 || tail == NULL || tail[3] != '\0') {
            fprintf(stderr, "FATAL: footer preset '%s' is not one well-formed HTML comment\n", p);
            fflush(stderr);
            ExitProcess(3);
          } else
            nchecks++;
        }
        for( ; *p != '\0' ; p++) {
          if (*p == '{' && p[1] == '{') {
            p++;                  /* the engine emits a literal brace for "{{" */
          } else if (*p == '{') {
            char name[32];                        /* longer than any field the engine expands */
            const char *const end = strchr(p + 1, '}');
            const size_t len = (end != NULL) ? (size_t) (end - p - 1) : sizeof(name);

            name[0] = '\0';
            if (len < sizeof(name))
              strncatbuff(name, p + 1, len);
            /* ask the engine's own expander, so a field added there needs no edit here */
            if (!hts_footer_field_ok(name)) {
              fprintf(stderr, "FATAL: footer preset '%s' names no engine field at '%s'\n",
                      FooterPresets[k], p);
              fflush(stderr);
              ExitProcess(3);
            } else
              nfields++;
            p = end;
          }
        }
      }
      printf("footer presets ok on %d checks and %d fields\n", nchecks, nfields);
    }
    int nlangs = 0;
    /* Walk the languages as the About box does: losing LANG_LOAD()'s empty-name
       terminator hangs it on the first run, past where --selftest ever reaches. */
    {
      const int LANG_SANE_MAX = 512;   /* lang.def ships a few dozen */
      const int saved = QLANG_T(-1);
      CStringArray names;
      int i;
      for(i=0 ; i<LANG_SANE_MAX ; i++) {
        char name[1024];
        QLANG_T(i);
        strcpybuff(name, "LANGUAGE_NAME");
        LANG_LOAD(name,sizeof(name));
        if (name[0] == '\0')
          break;
        /* The About box selects its entry by name, so a duplicate would select the wrong one. */
        for(INT_PTR j=0 ; j<names.GetSize() ; j++) {
          if (names[j] == name) {
            fprintf(stderr, "FATAL: languages %d and %d are both named '%s'\n", (int) j, i, name);
            fflush(stderr);
            ExitProcess(4);
          }
        }
        names.Add(name);
      }
      QLANG_T(saved);
      if (i >= LANG_SANE_MAX) {
        fprintf(stderr, "FATAL: the language list never ends: LANG_LOAD() lost its empty-name terminator\n");
        fflush(stderr);
        ExitProcess(4);
      }
      nlangs = i;
      printf("language list ends after %d entries\n", i);
    }
    /* lang.indexes seeds the first run from the system locale, but it numbers from
       LANGUAGE_1 while our indices start at 0: pin that offset against the list above. */
    {
      static const struct { const char* tag; const char* name; } expect[] = {
        { "en", "English" }, { "fr", "Francais" }, { "bg", "Bulgarian" },
        { "pt_br", "Portugues-Brasil" }, { NULL, NULL }
      };
      const int saved = QLANG_T(-1);
      int nchecks = 0;
      int k;
      for(k=0 ; expect[k].tag != NULL ; k++) {
        char name[1024];
        const int index = LANG_INDEX_OF(expect[k].tag);
        if (index < 0 || index >= nlangs) {
          fprintf(stderr, "FATAL: lang.indexes gives '%s' the unusable index %d\n",
                  expect[k].tag, index);
          fflush(stderr);
          ExitProcess(5);
        } else
          nchecks++;
        QLANG_T(index);
        strcpybuff(name, "LANGUAGE_NAME");
        LANG_LOAD(name,sizeof(name));
        if (strcmp(name, expect[k].name) != 0) {
          fprintf(stderr, "FATAL: lang.indexes maps '%s' to '%s', expected '%s'\n",
                  expect[k].tag, name, expect[k].name);
          fflush(stderr);
          ExitProcess(5);
        } else
          nchecks++;
      }
      QLANG_T(saved);
      if (LANG_INDEX_OF("zz") >= 0) {
        fprintf(stderr, "FATAL: lang.indexes matched the bogus tag 'zz'\n");
        fflush(stderr);
        ExitProcess(5);
      } else
        nchecks++;
      printf("lang.indexes offset checked on %d locales and %d checks\n", k, nchecks);
    }
    /* Assert the URL OnUpdate() builds, not the macro it reads: checking LANGUAGE_FILE
       here would still pass if the call site went back to LANGUAGE_NAME. */
    {
      static const struct { const char* tag; const char* file; int same_as_name; } expect[] = {
        { "en", "English", 1 }, { "fi", "Finnish", 0 }, { "uk", "Ukrainian", 0 }, { NULL, NULL, 0 }
      };
      const int saved = QLANG_T(-1);
      int nchecks = 0;
      int k;
      for(k=0 ; expect[k].tag != NULL ; k++) {
        CString url;
        CString want;
        const char* name;
        int at;
        const int index = LANG_INDEX_OF(expect[k].tag);
        if (index < 0 || index >= nlangs) {
          fprintf(stderr, "FATAL: lang.indexes gives '%s' the unusable index %d\n", expect[k].tag, index);
          fflush(stderr);
          ExitProcess(6);
        }
        /* QLANG_T, not LANG_T: LANG_T persists the choice to the registry. */
        QLANG_T(index);
        LANG_LOAD(NULL, 0);
        url = UpdateUrl();
        name = LANGUAGE_NAME;
        want.Format("&Language=%s", expect[k].file);
        at = url.Find(want);
        if (at < 0) {
          fprintf(stderr, "FATAL: '%s' builds '%s', wanted it to carry '%s'\n",
                  expect[k].tag, (LPCTSTR) url, (LPCTSTR) want);
          fflush(stderr);
          ExitProcess(6);
        }
        nchecks++;
        if (url.Find(want, at + 1) >= 0) {
          fprintf(stderr, "FATAL: '%s' carries '%s' more than once\n", expect[k].tag, (LPCTSTR) want);
          fflush(stderr);
          ExitProcess(6);
        }
        nchecks++;
        /* The endonyms are what broke this, so the whole URL must stay ASCII. */
        {
          const char* p;
          for(p = (LPCTSTR) url ; *p != '\0' ; p++) {
            if ((unsigned char) *p > 0x7f) {
              fprintf(stderr, "FATAL: '%s' builds the non-ASCII '%s'\n", expect[k].tag, (LPCTSTR) url);
              fflush(stderr);
              ExitProcess(6);
            }
          }
        }
        nchecks++;
        /* If these stopped diverging the endonym change was reverted, and the rows above went quiet. */
        if ((strcmp(expect[k].file, name) == 0) != (expect[k].same_as_name != 0)) {
          fprintf(stderr, "FATAL: '%s' has LANGUAGE_NAME '%s' and LANGUAGE_FILE '%s'\n",
                  expect[k].tag, name, expect[k].file);
          fflush(stderr);
          ExitProcess(6);
        }
        nchecks++;
      }
      printf("update-url language ok on %d locales and %d checks\n", k, nchecks);

      /* Three rows cannot see a catalog that lost the key: LANG_LOAD backfills undefined
         strings from English, so the wire would carry "English" for it. A duplicate is
         exactly that collision, so uniqueness is the check that catches it. */
      {
        CStringArray files;
        int i;
        for(i = 0 ; i < nlangs ; i++) {
          CString f;
          const char* q;
          INT_PTR j;
          QLANG_T(i);
          LANG_LOAD(NULL, 0);
          f = LANGUAGE_FILE;
          if (f.IsEmpty()) {
            fprintf(stderr, "FATAL: language %d has no LANGUAGE_FILE\n", i);
            fflush(stderr);
            ExitProcess(6);
          }
          for(q = (LPCTSTR) f ; *q != '\0' ; q++) {
            if ((unsigned char) *q > 0x7f) {
              fprintf(stderr, "FATAL: language %d has the non-ASCII LANGUAGE_FILE '%s'\n", i, (LPCTSTR) f);
              fflush(stderr);
              ExitProcess(6);
            }
          }
          for(j = 0 ; j < files.GetSize() ; j++) {
            if (files[j] == f) {
              fprintf(stderr, "FATAL: languages %d and %d both send '%s'\n", (int) j, i, (LPCTSTR) f);
              fflush(stderr);
              ExitProcess(6);
            }
          }
          files.Add(f);
        }
        printf("update-url basename unique and ASCII on %d locales\n", i);
      }
      QLANG_T(saved);
      LANG_LOAD(NULL, 0);
    }
    /* The header is a bug report's only statement of what crashed, and no other check sees it. */
    {
      static const char lead[] = "WinHTTrack " WINHTTRACK_VERSION " (";
      const char *const header = CrashReportHeader();
      /* From sizeof(void*), not from the macro the header is built from: a compiler ladder
         that mis-fires prints "unknown" on both builds, and only an outside oracle sees it. */
      const char *const arch = (sizeof(void*) == 8) ? "x64" : "x86";
      const size_t archlen = strlen(arch);
      int nchecks = 0;

      /* Anchored, not searched: the engine reaching 3.50.x makes its version a substring
         of ours, and a search would then take either version for the other. */
      if (strncmp(header, lead, sizeof(lead) - 1) != 0) {
        fprintf(stderr, "FATAL: crash report header '%s' does not open with '%s'\n", header, lead);
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      if (strncmp(header + sizeof(lead) - 1, arch, archlen) != 0) {
        fprintf(stderr, "FATAL: crash report header '%s' does not name the %s build\n", header, arch);
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      if (strcmp(header + sizeof(lead) - 1 + archlen, ", engine " HTTRACK_VERSIONID ")") != 0) {
        fprintf(stderr, "FATAL: crash report header '%s' does not end on engine "
                HTTRACK_VERSIONID "\n", header);
        fflush(stderr);
        ExitProcess(3);
      } else
        nchecks++;
      /* Pinned where the count is produced: a truncated list runs nothing and still prints. */
      if (nchecks != 3) {
        fprintf(stderr, "FATAL: crash report header ran %d checks, expected 3\n", nchecks);
        fflush(stderr);
        ExitProcess(3);
      }
      printf("crash report header ok on %d checks\n", nchecks);
    }
    /* Exercise the crash reporter for real: a Release PDB built without line info, or a
       first-chance hook that never registered, both still produce a plausible-looking
       report that names nothing. Only throwing proves the chain resolves. */
    TRY {
      AfxThrowInvalidArgException();
    } CATCH_ALL(e) {
      TCHAR reason[256];
      if (!e->GetErrorMessage(reason, _countof(reason))) {
        _tcscpy_s(reason, _countof(reason), _T("(no description)"));
      }
      CrashReportLogException(reason);
    } END_CATCH_ALL
    /* Printed through the reporter's own header, so the line CI checks exercises it. */
    printf("%s: startup ok\n", CrashReportHeader());
    fflush(stdout);
    ExitProcess(0);
  }
  
  /* INDISPENSABLE pour le drag&drop! */
  InitCommonControls();
  if (!AfxOleInit())
  {
	  AfxMessageBox(LANG(LANG_F1));
	  return FALSE;
  }
  AfxEnableControlContainer();
  
  // Pointeur sur CShellApp
  CShellApp_app=&app;
  this_app=this;
  _Cinprogress_inst=NULL;
  LibRasUse=0;

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

  httrack_icon=AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

  // DOC //
	CMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMultiDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CWinHTTrackDoc),
		RUNTIME_CLASS(CSplitterFrame),       // main SDI frame window
		RUNTIME_CLASS(CView)); 
	AddDocTemplate(pDocTemplate);

  /*
	CMDIFrameWnd* pMainFrame = new CMDIFrameWnd;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
  */

	// create main window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;
	int nCmdShow = m_nCmdShow;


  // Also in this example, there is only one menubar shared between
	//  all the views.  The automatic menu enabling support of MFC
	//  will disable the menu items that don't apply based on the
	//  currently active view.  The one MenuBar is used for all
	//  document types, including when there are no open documents.

  // enable file manager drag/drop and DDE Execute open
	pMainFrame->DragAcceptFiles();

  // Now finally show the main menu
	//pMainFrame->ShowWindow(m_nCmdShow);
	//pMainFrame->UpdateWindow();
	m_pMainWnd = pMainFrame;

  // command line arguments are ignored, create a new (empty) document
	//OnFileNew();
  // DOC //

  // Parse command line for standard shell commands, DDE, file open
  CCommandLineInfo cmdInfo;
  ParseCommandLine(cmdInfo);

  TCHAR ModulePath[MAX_PATH + 1];
  ModulePath[0] = '\0';
  ::GetModuleFileName(NULL, ModulePath, sizeof(ModulePath)/sizeof(TCHAR) - 1);
  hts_rootdir(ModulePath);

  // Restore position
	((CMainFrame*)m_pMainWnd)->InitialShowWindow(nCmdShow);
	pMainFrame->UpdateWindow();

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

  // Init Winsock
  WSockInit();

	// The one and only window has been initialized, so show and update it.
	//m_pMainWnd->ShowWindow(SW_SHOW);
	//m_pMainWnd->UpdateWindow();

  /*CWinApp* app=AfxGetApp();
  POSITION pos;
  pos=app->GetFirstDocTemplatePosition();
  CDocTemplate* templ = app->GetNextDocTemplate(pos);
  pos=templ->GetFirstDocPosition();
  CDocument* doc  = templ->GetNextDoc(pos);

  CRuntimeClass* pRuntimeClass = RUNTIME_CLASS( CTest );
  CObject* pObject = pRuntimeClass->CreateObject();
  ASSERT( pObject->IsKindOf( RUNTIME_CLASS( CTest ) ) );
  
  doc->AddView((CView*) pObject);
  */

  {
    // enable file manager drag/drop and DDE Execute open
    EnableShellOpen();

    CWinApp* pApp = AfxGetApp();

    // Portable runs always associate; an installed one obeys the setup's file-types task.
    if (pApp->GetProfileInt("Interface","SetupRun",0) != 1
      || pApp->GetProfileInt("Interface","SetupHasRegistered",0) == 1) {
        HKEY phkResult;
        DWORD creResult;

      RegisterShellFileTypes();

      // register "New File" handler
      if (RegCreateKeyEx(HKEY_CLASSES_ROOT,".whtt",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&phkResult,&creResult)==ERROR_SUCCESS) {
        RegCloseKey(phkResult);
        if (RegCreateKeyEx(HKEY_CLASSES_ROOT,".whtt\\ShellNew",0,NULL,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&phkResult,&creResult)==ERROR_SUCCESS) {
          char voidbuff='\0';
          RegSetValueEx(phkResult,"NullFile",0,REG_SZ,(LPBYTE)&voidbuff,1);
          RegCloseKey(phkResult);
        }
      }   
    }

    // Infos la 1ere fois!
    if (pApp->GetProfileInt("Interface","FirstRun",0) != 3) {
      pApp->WriteProfileInt("Interface","FirstRun",3);

      Cabout about;
      about.DoModal();
      
      // Default proxy? Check is the current IP looks local or not.
      BOOL isPublic = FALSE;
      char hostname[256];
      if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        struct addrinfo* res = NULL;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
          if (res->ai_addr != NULL && res->ai_addrlen > 0) {
            if (res->ai_family == AF_INET) {
              sockaddr_in *const si = (sockaddr_in*) res->ai_addr;
              const unsigned char *const ipv4 = (unsigned char*) &si->sin_addr;
              isPublic = ! (
                ipv4[0] == 10  /* 10/8 */
                || (ipv4[0] == 192 && ipv4[1] == 168)  /* 192.168/16 */
                || (ipv4[0] == 172 && ipv4[1] >= 16 && ipv4[1] <= 31)  /* 172.16/12 */
                );
            } else if (res->ai_family == AF_INET6) {  /* assume no more proxy */
              isPublic = TRUE;
            }
          }
        }
        if (res) {
          freeaddrinfo(res);
        }
      }
      if (!isPublic && maintab) {
        maintab->DefineDefaultProxy();
        if (maintab->DoModal()!=IDCANCEL) {
          // Default proxy values
          CString strSection       = "OptionsValues";
          MyWriteProfileString("",strSection, "Proxy",maintab->m_option10.m_proxy);
          MyWriteProfileString("",strSection, "Port",maintab->m_option10.m_port);
        }
        maintab->UnDefineDefaultProxy();
      }
    }
  }
  

#ifdef HTTRACK_AFF_WARNING
#ifndef _DEBUG
  AfxMessageBox("--WARNING--\r\n"HTTRACK_AFF_WARNING);
#endif
#endif

  /* Last, and on a thread of its own: none of it may delay startup. OnIdle() collects
     the answer. */
  WhttSigCheckStart(m_pMainWnd != NULL ? m_pMainWnd->GetSafeHwnd() : NULL);

  return TRUE;
}


/* Where the signature verdict reaches the user. Only while the main window is enabled:
   MFC disables it under a modal dialog, and opening a second one on top of the first is
   how this program has broken before. The one-shot is not consumed until then, so the
   warning waits rather than being lost. */
BOOL CWinHTTrackApp::OnIdle(LONG lCount) {
  CWnd *const main = AfxGetMainWnd();

  if (main != NULL && main->IsWindowEnabled() && WhttSigTakeWarning()) {
    CUnofficial warning(main);
    warning.DoModal();
  }
  return CWinApp::OnIdle(lCount);
}

BOOL CWinHTTrackApp::WSockInit() {
  // Initialiser WINSOCK
  WORD   wVersionRequested; /* requested version WinSock API */ 
  WSADATA wsadata;        /* Windows Sockets API data */
  {
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      //HTS_PANIC_PRINTF("Winsock not found!\n");
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      //HTS_PANIC_PRINTF("WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
    }
  }
  // Fin Initialiser WINSOCK
  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

/*
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
*/

// App command to run the dialog
void CWinHTTrackApp::OnAppAbout()
{
  Cabout about;
  about.DoModal();
//	CAboutDlg aboutDlg;
//	aboutDlg.DoModal();
}


/* Request: Set new view in the splitter window (when clicking on finished, for example) */
/*extern "C" {
  int hts_resetvar(void);
}*/

void CWinHTTrackApp::OnViewRestart() {
  //CloseAllDocuments(FALSE);
  //OnFileNew();

  /* Free library */
  WHTT_LOCK();
  //hts_resetvar();
  WHTT_UNLOCK();

  this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
}

void CWinHTTrackApp::OnWizRequest1() {
  wizard diawiz;
  diawiz.m_question=WIZ_question;
  diawiz.DoModal();
  // User-typed, so clip rather than abort on a long one.
  WIZ_reponse[0] = '\0';
  strlncatbuff(WIZ_reponse, (LPCTSTR) diawiz.m_reponse, sizeof(WIZ_reponse), sizeof(WIZ_reponse) - 1);
}

void CWinHTTrackApp::OnWizRequest2() {
  wizard2 diawiz2;
  diawiz2.m_question=WIZ_question;
  if (diawiz2.DoModal()==IDOK)
  strcpybuff(WIZ_reponse,"YES");
  else
    strcpybuff(WIZ_reponse,"NO");
}

void CWinHTTrackApp::OnWizRequest3() {
  WizLinks diawiz3;
  diawiz3.m_url=WIZ_question;
  if (diawiz3.DoModal()==IDskipall)
    strcpybuff(WIZ_reponse,"*");
  else
    WizLinkAnswer(diawiz3.m_lnk, diawiz3.m_scope, WIZ_reponse, sizeof(WIZ_reponse));
}


//


/////////////////////////////////////////////////////////////////////////////
// CWinHTTrackApp message handlers

// Ne fait pas partie de la classe
/*
UINT RunBackEngine( LPVOID pP ) {
  static int running=0;
  if (running)
    return 0;
  running=1;
  {
    CWinApp* app=AfxGetApp();
    POSITION pos;
    pos=app->GetFirstDocTemplatePosition();
    CDocTemplate* templ = app->GetNextDocTemplate(pos);
    pos=templ->GetFirstDocPosition();
    CDocument* doc  = templ->GetNextDoc(pos);
    pos=doc->GetFirstViewPosition();
    CView*     view = doc->GetNextView(pos);
    App_Main_HWND=view->m_hWnd;
  }
  //
  CShellApp app;
  app.InitInstance();
  running=0;
  return 0;
}
*/

/*
void CWinHTTrackApp::OnWizard() {
  //this_CSplitterFrame->SetNewView(0,1,RUNTIME_CLASS(CDialogContainer));
}
*/

afx_msg void CWinHTTrackApp::OnFileNew( ) {
  OpenDocumentFile("");
}

afx_msg void CWinHTTrackApp::OnFileOpen( ) {
  this->CWinApp::OnFileOpen();
}

void CWinHTTrackApp::OnFileSave() {
}

void CWinHTTrackApp::OnFileSaveAs() 
{
	// TODO: Add your command handler code here
	
}

void CWinHTTrackApp::OnFileDelete()
{
  static char szFilter[256];
  strcpybuff(szFilter,"WinHTTrack Website Copier Project (*.whtt)|*.whtt||");
  CFileDialog* dial = new CFileDialog(true,"whtt",NULL,OFN_HIDEREADONLY,szFilter);
  if (dial->DoModal() == IDOK) {
    CString st=dial->GetPathName();
    if (fexist((char*) LPCTSTR(st))) {
      int pos=st.ReverseFind('.');
      CString dir=st.Left(pos)+"\\";
      char msg[1000];
      sprintf(msg,"%s\r\n%s",LANG_DELETECONF,(LPCTSTR)dir);
      if (AfxMessageBox(msg,MB_OKCANCEL)==IDOK) {
        if (remove(st)) {
          AfxMessageBox("Error deleting "+st);
        } else {
          RmDir(dir);
        }
      }
    } else
      AfxMessageBox(LANG(LANG_G26 /*"File not found!","Fichier introuvable!"*/));
  }
  delete dial;
}

void CWinHTTrackApp::OnBrowseWebsites()
{
  CString st=dialog0->GetBasePath();

  if (st.GetLength()<=1) {
    CString strSection       = "DefaultValues";    
    CWinApp* pApp = AfxGetApp();
    st = pApp->GetProfileString(strSection, "BasePath");
    st += "\\";
  }

  st+="index.html";
  ShellExecute(NULL,"open",st,"","",SW_RESTORE);	
}

BOOL CWinHTTrackApp::RmDir(CString srcpath) {
  CWaitCursor wait;

  if (srcpath.GetLength()==0)
    return FALSE;

  // A junction or symlink must be unlinked, never descended: its target is outside what the user confirmed.
  const DWORD attr = GetFileAttributes(srcpath);
  if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
    if (!RemoveDirectory(srcpath)) {
      AfxMessageBox("Error deleting "+srcpath);
      return FALSE;
    }
    return TRUE;
  }

  CString path=srcpath;
  WIN32_FIND_DATA find;
  BOOL ok = TRUE;
  if (path.Right(1)!="\\")
    path+="\\";
  HANDLE h = FindFirstFile(path+"*.*",&find);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_SYSTEM ))
      if (strcmp(find.cFileName,".."))
      if (strcmp(find.cFileName,"."))
        if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY )) {
          if (remove(path+find.cFileName)) {
            AfxMessageBox("Error deleting "+path+find.cFileName);
            ok = FALSE;
          }
        } else {
          if (!RmDir(path+find.cFileName))
            ok = FALSE;
        }
    } while(ok && FindNextFile(h,&find));
    FindClose(h);
  }
  if (!ok)
    return FALSE;
  if (rmdir(srcpath)) {
    AfxMessageBox("Error deleting "+srcpath);
    return FALSE;
  }
  return TRUE;
}


void CWinHTTrackApp::OnFileMruFile1() 
{
	// TODO: Add your command handler code here
	
}

void CWinHTTrackApp::Onipabout() 
{
  Cabout about;
  about.DoModal();
}

void CWinHTTrackApp::OnUpdate() 
{
  const CString st = UpdateUrl();
  if (!ShellOpen(st, SW_SHOWNORMAL))
    AfxMessageBox("Cannot open a web browser for " + st);
}

// Appel aide
void CWinHTTrackApp::OnHelpInfo2() {
  (void) OnHelpInfo(NULL);
}

BOOL CWinHTTrackApp::OnHelpInfo(HELPINFO* dummy) 
{
  HtsHelper->HelpTopic("step-address");
  return true;
}

// Forwards

void CWinHTTrackApp::FwOnhide() {
  if (this_CSplitterFrame)
    this_CSplitterFrame->Onhide();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnLoadprofile() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnLoadprofile();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnSaveprofile() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnSaveprofile();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnLoaddefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnLoaddefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnSavedefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnSavedefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}
void CWinHTTrackApp::FwOnResetdefault() {
  if ((dialog1!=NULL) && (maintab!=NULL))
    dialog1->OnResetdefault();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

//

void CWinHTTrackApp::FwOnModifyOpt() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnModifyOpt();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnPause() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnPause();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOniplogLog() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OniplogLog();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOniplogErr() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OniplogErr();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

void CWinHTTrackApp::FwOnViewTransfers() {
  if ((inprogress!=NULL) && (maintab!=NULL))
    inprogress->OnViewTransfers();
  else
    AfxMessageBox(LANG_ACTIONNYP,MB_OK);
}

CDocument* CWinHTTrackApp::OpenDocumentFile( LPCTSTR lpszFileName)
{
  // Eviter deux fenêtres (un seul document)
  // Le CMultui..->CSingleDoc.. est trop complexe à changer (à cause du splitter-wnd)
  int count=1;

  { /* Check if a document exists, and if exists if empty or not, and if name is different */
    POSITION pos;
    pos=GetFirstDocTemplatePosition();
    if (pos) {
      CDocTemplate* tmpl=GetNextDocTemplate(pos);
      if (tmpl) {
        pos=tmpl->GetFirstDocPosition();
        if (pos) {
          CDocument* doc  = tmpl->GetNextDoc(pos);
          if (doc) {
            if (dialog0->GetName().GetLength()==0) {
              CloseAllDocuments(FALSE);
              count=0;        /* No documents */
            } else {
              if (dialog0->GetPath0()+".whtt" == LPCSTR(lpszFileName))
                return NULL;
            }
          }
        } else
          count=0;          /* No documents */
      }
    }
  }

  // Ouvrir nouvelle instance
  if (count) {
    char cmdl[2048];
    TCHAR ModulePath[MAX_PATH + 1];
    ModulePath[0] = '\0';
    ::GetModuleFileName(NULL, ModulePath, sizeof(ModulePath)/sizeof(TCHAR) - 1);
    CString name = ModulePath;
    strcpybuff(cmdl,"\"");
    strcatbuff(cmdl,lpszFileName);
    strcatbuff(cmdl,"\"");
    ShellExecute(NULL,"open",name,cmdl,"",SW_RESTORE);
    return NULL;
  }

  // Ouvrir nouveau?
  //if (count)
  //  return;       // ne rien faire, car limité à 1 document
  //count++;

  /* Ouvrir */
  /*
  CWinApp* app=AfxGetApp();
  POSITION pos;
  pos=app->GetFirstDocTemplatePosition();
  CDocTemplate* templ = app->GetNextDocTemplate(pos);
  pos=templ->GetFirstDocPosition();
  if (pos) {
    CDocument* doc  = templ->GetNextDoc(pos);
    if (doc)
      if (!doc->SaveModified())
        return NULL;
  }
  CloseAllDocuments(FALSE);
  */
  if (strlen(lpszFileName))
    return CWinApp::OpenDocumentFile(lpszFileName);
  else
    CWinApp::OnFileNew();
  return NULL;
}

void CWinHTTrackApp::NewTabs() {
  DeleteTabs();
  m_tab0 = new CFirstInfo();
  m_tab1 = new CNewProj();
  m_tab2 = new Wid1();
  m_tab3 = new Ctrans();
  m_tabprogress = new Cinprogress();
  m_tabend = new Cinfoend();
}

void CWinHTTrackApp::DeleteTabs() {
  if (m_tab0)
  if (m_tab0->GetSafeHwnd())       /* a déja été détruit par CWinApp */
    delete m_tab0;
  if (m_tab1)
  if (m_tab1->GetSafeHwnd())
    delete m_tab1;
  if (m_tab2)
  if (m_tab2->GetSafeHwnd())
    delete m_tab2;
  if (m_tab3)
  if (m_tab3->GetSafeHwnd())
    delete m_tab3;
  if (m_tabprogress)
  if (m_tabprogress->GetSafeHwnd())
    delete m_tabprogress;
  if (m_tabend)
  if (m_tabend->GetSafeHwnd())
    delete m_tabend;

  m_tab0=NULL;
  m_tab1=NULL;
  m_tab2=NULL;
  m_tab3=NULL;
  m_tabprogress=NULL;
  m_tabend=NULL;
}

// MFC's box for a thrown exception names neither the call nor the argument; a trace does.
LRESULT CWinHTTrackApp::ProcessWndProcException(CException* e, const MSG* pMsg) {
  TCHAR reason[512];
  CString what;

  if (e == NULL || !e->GetErrorMessage(reason, _countof(reason))) {
    _tcscpy_s(reason, _countof(reason), _T("(no description)"));
  }
  what.Format("%s [window message 0x%04x]", reason,
              pMsg != NULL ? (unsigned) pMsg->message : 0u);
  CrashReportLogException((LPCTSTR) what);

  return CWinApp::ProcessWndProcException(e, pMsg);
}

int CWinHTTrackApp::ExitInstance() 
{
  LANG_DELETE();

  /* Uninitialize */
  htsthread_wait();
  hts_uninit();

  return CWinApp::ExitInstance();
}
