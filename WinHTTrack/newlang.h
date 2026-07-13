
#ifndef HTS_DEFNEWLANG
#define HTS_DEFNEWLANG

void LANG_LOAD(char* limit_to);
void LANG_INIT();
int LANG_T(int);
int QLANG_T(int l);
//char* LANGSEL(char* lang0,...);
/* Set by --selftest (WinHTTrack.cpp): report fatals on stderr and exit non-zero
   instead of raising a message box nobody can click. */
extern int WhttSelfTest;
void WhttEnsureConsole(void);

const char* LANGSEL(const char* name);
const char* LANGINTKEY(const char* name);
void LANG_DELETE();
void conv_printf(char* from,char* to);
#define LANG(A) A

BOOL SetDlgItemTextCP(HWND hDlg, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextCP(CWnd* wnd, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextUTF8(HWND hDlg, int nIDDlgItem, LPCSTR lpString);
BOOL SetDlgItemTextUTF8(CWnd* wnd, int nIDDlgItem, LPCSTR lpString);
BOOL SetWindowTextCP(HWND hWnd, LPCSTR lpString);
BOOL SetWindowTextCP(CWnd* wnd, LPCSTR lpString);
BOOL SetWindowTextUTF8(HWND hWnd, LPCSTR lpString);
BOOL SetWindowTextUTF8(CWnd* wnd, LPCSTR lpString);
BOOL ModifyMenuCP(HMENU hMnu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem);
BOOL ModifyMenuCP(CMenu* menu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem);

#endif

