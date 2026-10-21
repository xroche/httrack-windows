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
/*       WhttBrowserLogin.dll entry points                      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <windows.h>
#include <objbase.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include <wrl.h>

/* WebView2LoaderStatic.lib defines what this declares, and vcpkg.json beside this file
   says why the static loader is the one we link. */
#include <WebView2.h>

#include "WhttBrowserLogin.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

unsigned __cdecl WhttBrowserLoginAbi(void) {
  return WHTT_BROWSER_LOGIN_ABI;
}

int __cdecl WhttBrowserLoginAvailable(char *version, size_t version_size) {
  char utf8[WHTT_BROWSER_LOGIN_VERSION_MAX * 2];
  LPWSTR found = NULL;

  utf8[0] = '\0';
  if (version != NULL && version_size != 0) {
    version[0] = '\0';
  }
  /* NULL means the Evergreen runtime, wherever it sits. A failure says none is installed,
     which is an answer rather than an error, because nothing here is the user's to fix. */
  const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &found);
  if (SUCCEEDED(hr) && found != NULL
      && WideCharToMultiByte(CP_UTF8, 0, found, -1, utf8, (int) sizeof(utf8), NULL, NULL) <= 0) {
    utf8[0] = '\0';
  }
  if (found != NULL) {
    CoTaskMemFree(found);
  }
  /* No runtime and a version we cannot read are the same answer to the caller. */
  if (utf8[0] == '\0') {
    return 0;
  }
  if (version != NULL && version_size != 0) {
    strncpy_s(version, version_size, utf8, _TRUNCATE);
  }
  return 1;
}

/* ------------------------------------------------------------ */
/* The browser window                                            */
/* ------------------------------------------------------------ */

/* Asks the window to tear itself down from the message loop. See requestFinish(). */
#define WHTT_WM_FINISH  (WM_APP + 1)

#define WHTT_ID_ADDRESS 100
#define WHTT_ID_GO      101
#define WHTT_ID_DONE    102

static const wchar_t WHTT_WINDOW_CLASS[] = L"WhttBrowserLoginWindow";

/* The window class and its children belong to this DLL, not to whoever loaded it. */
extern "C" IMAGE_DOS_HEADER __ImageBase;
#define WHTT_INSTANCE ((HINSTANCE) &__ImageBase)

/* One cookie, copied out of the browser before the list it came in is released. */
struct HarvestedCookie {
  std::string name, value, domain, path;
  int secure = 0;
  double expires = 0.0;
};

/* Everything one WhttBrowserLoginRun() call owns. The window carries a pointer to it in
   GWLP_USERDATA, and it outlives the window, because the cookies go to the caller once
   the window has gone. Shared, because a browser callback can land after the user has
   closed the window and would otherwise write into a stack frame that is gone. */
struct LoginWindow : std::enable_shared_from_this<LoginWindow> {
  const WhttBrowserLoginRequest *request = NULL;
  HWND frame = NULL, address = NULL, go = NULL, done = NULL;
  HWND disabledParent = NULL;   /* the owner we greyed out, put back before the frame dies */
  HFONT font = NULL;
  int dpi = 96;
  ComPtr<ICoreWebView2Controller> controller;
  ComPtr<ICoreWebView2> webview;
  bool harvesting = false;
  bool closing = false;     /* the teardown has begun, and it must not be re-entered */
  bool gaveUp = false;      /* stopped waiting for the browser, which may still answer */
  bool answered = false;    /* the browser handed its cookies over, even if it had none */
  bool finished = false;
  int result = WhttBrowserLoginDone;
  std::string userAgent;
  std::vector<HarvestedCookie> cookies;
};

/* UTF-8 out of what the browser handed us. Empty on anything we cannot convert, because
   the caller treats every one of these fields as optional. */
static std::string utf8Of(LPCWSTR w) {
  if (w == NULL || *w == L'\0')
    return std::string();

  const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  if (n <= 1)
    return std::string();

  std::string out((size_t) (n - 1), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], n, NULL, NULL) <= 0)
    out.clear();
  return out;
}

static std::wstring wideOf(const char *utf8) {
  if (utf8 == NULL || *utf8 == '\0')
    return std::wstring();

  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (n <= 1)
    return std::wstring();

  std::wstring out((size_t) (n - 1), L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n) <= 0)
    out.clear();
  return out;
}

/* Reads one COM-allocated string and frees it. */
static std::string takeString(LPWSTR owned) {
  const std::string out = utf8Of(owned);

  if (owned != NULL)
    CoTaskMemFree(owned);
  return out;
}

static int scaled(const LoginWindow *s, int px) {
  return MulDiv(px, s->dpi, 96);
}

static void layoutWindow(LoginWindow *s) {
  RECT rc;

  if (s->frame == NULL || !GetClientRect(s->frame, &rc))
    return;

  const int pad = scaled(s, 6);
  const int row = scaled(s, 24);
  const int button = scaled(s, 72);
  const int width = rc.right - rc.left;
  const int doneX = width - pad - button;
  const int goX = doneX - pad - button;
  const int editW = goX - pad - pad;

  if (editW > 0)
    MoveWindow(s->address, pad, pad, editW, row, TRUE);
  MoveWindow(s->go, goX, pad, button, row, TRUE);
  MoveWindow(s->done, doneX, pad, button, row, TRUE);

  if (s->controller) {
    RECT view = rc;

    view.top = pad + row + pad;
    if (view.top > view.bottom)
      view.top = view.bottom;
    s->controller->put_Bounds(view);
  }
}

static bool ourControl(const LoginWindow *s, HWND window) {
  return window == s->address || window == s->go || window == s->done;
}

/* Closes the browser and the window, in that order, and puts the owner back first so
   Windows activates it rather than whatever application sits behind us.

   ONLY the message loop may call this, never a browser callback: see requestFinish(). */
static void finishWindow(LoginWindow *s) {
  /* Close() below can pump, which would deliver a finish already posted and bring us
     straight back in here on a controller we are in the middle of closing. */
  if (s->closing)
    return;
  s->closing = true;
  /* Before the window that hosts it, which is the order Microsoft's own Win32 sample
     uses. Closing it from inside DestroyWindow tears down its child windows while
     Windows is already destroying their parent. */
  if (s->controller) {
    s->controller->Close();
    s->controller.Reset();
    s->webview.Reset();
  }
  if (s->disabledParent != NULL) {
    EnableWindow(s->disabledParent, TRUE);
    s->disabledParent = NULL;
  }
  if (s->frame != NULL)
    DestroyWindow(s->frame);
  else
    s->finished = true;   /* nothing left to end the loop, so end it here */
}

/* Asks for the teardown rather than doing it, for a caller that is a browser callback.
   The browser resumes its own dispatch after such a callback returns, so closing it from
   inside one leaves it running on an object we already destroyed. That crashed a real
   build: the window shut, and the fault came afterwards, inside the browser's code. */
static void requestFinish(LoginWindow *s) {
  if (s->frame != NULL)
    PostMessageW(s->frame, WHTT_WM_FINISH, 0, 0);
  else
    s->finished = true;
}

static void collectCookies(LoginWindow *s, ICoreWebView2CookieList *list) {
  UINT count = 0;

  if (list == NULL || FAILED(list->get_Count(&count)))
    return;
  for(UINT i = 0 ; i < count ; i++) {
    ComPtr<ICoreWebView2Cookie> cookie;
    HarvestedCookie out;
    LPWSTR text = NULL;
    BOOL flag = FALSE;
    double expires = 0.0;

    if (FAILED(list->GetValueAtIndex(i, &cookie)) || !cookie)
      continue;
    if (SUCCEEDED(cookie->get_Name(&text)))
      out.name = takeString(text);
    if (SUCCEEDED(cookie->get_Value(&text)))
      out.value = takeString(text);
    if (SUCCEEDED(cookie->get_Domain(&text)))
      out.domain = takeString(text);
    if (SUCCEEDED(cookie->get_Path(&text)))
      out.path = takeString(text);
    out.secure = SUCCEEDED(cookie->get_IsSecure(&flag)) && flag ? 1 : 0;
    /* A session cookie reports -1, which is already how the caller spells one. */
    out.expires = SUCCEEDED(cookie->get_Expires(&expires)) ? expires : 0.0;
    s->cookies.push_back(out);
  }
}

/* Asks for every cookie the profile holds, then closes the window once they are in.
   Closing cannot wait for the user, so the window stays up until the answer lands. */
static void beginHarvest(const std::shared_ptr<LoginWindow> &s) {
  ComPtr<ICoreWebView2_2> webview2;
  ComPtr<ICoreWebView2CookieManager> jar;

  if (s->harvesting)
    return;
  s->harvesting = true;
  /* Nothing more to ask of the window while the browser answers. */
  EnableWindow(s->address, FALSE);
  EnableWindow(s->go, FALSE);
  EnableWindow(s->done, FALSE);
  if (!s->webview || FAILED(s->webview.As(&webview2))
      || FAILED(webview2->get_CookieManager(&jar)) || !jar) {
    finishWindow(s.get());
    return;
  }
  /* No URI, so this takes the whole profile and not one site. BrowserLoginHarvest in
     WinHTTrack/browser-login.h says why. */
  const HRESULT hr = jar->GetCookies(NULL,
    Callback<ICoreWebView2GetCookiesCompletedHandler>(
      [s](HRESULT result, ICoreWebView2CookieList *list) -> HRESULT {
        if (SUCCEEDED(result)) {
          s->answered = true;
          collectCookies(s.get(), list);
        }
        requestFinish(s.get());
        return S_OK;
      }).Get());

  if (FAILED(hr))
    finishWindow(s.get());
}

static void navigateFromAddress(LoginWindow *s) {
  wchar_t typed[2048];

  if (!s->webview || GetWindowTextW(s->address, typed, 2048) == 0)
    return;

  std::wstring uri(typed);

  /* Navigate() refuses a bare host, which is most of what anyone types. The test is the
     one Wid1 uses on the address box, so a port number does not read as a scheme. */
  if (uri.find(L":/") == std::wstring::npos)
    uri = L"http://" + uri;
  s->webview->Navigate(uri.c_str());
}

static LRESULT CALLBACK loginWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  LoginWindow *const s = (LoginWindow *) GetWindowLongPtrW(window, GWLP_USERDATA);

  switch(message) {
  case WM_NCCREATE:
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      (LONG_PTR) ((CREATESTRUCTW *) lparam)->lpCreateParams);
    break;
  case WHTT_WM_FINISH:
    if (s != NULL)
      finishWindow(s);
    return 0;
  case WM_SIZE:
    if (s != NULL)
      layoutWindow(s);
    return 0;
  /* Our frame is no dialog, so IsDialogMessage() asks us which button Enter presses. */
  case DM_GETDEFID:
    return MAKELRESULT(WHTT_ID_GO, DC_HASDEFID);
  case WM_COMMAND:
    if (s != NULL) {
      if (LOWORD(wparam) == WHTT_ID_DONE) {
        /* Not WM_CLOSE: a double click would send two, and the second is what gives up
           on the harvest the first one started. */
        beginHarvest(s->shared_from_this());
        return 0;
      }
      if (LOWORD(wparam) == WHTT_ID_GO) {
        navigateFromAddress(s);
        return 0;
      }
    }
    break;
  case WM_SETFOCUS:
    if (s != NULL && s->controller)
      s->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    return 0;
  case WM_CLOSE:
    if (s != NULL) {
      /* The window goes once the cookies are out of the browser. Closing it again
         gives up on them, because a browser that died still owes the answer. */
      if (s->harvesting) {
        s->gaveUp = true;
        finishWindow(s);
      } else
        beginHarvest(s->shared_from_this());
      return 0;
    }
    break;
  case WM_DESTROY:
    /* No browser work here. finishWindow() has already closed it, which is what keeps
       the close out of the middle of DestroyWindow. */
    if (s != NULL)
      s->frame = NULL;
    return 0;
  case WM_NCDESTROY:
    if (s != NULL)
      s->finished = true;
    break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

static bool registerWindowClass(void) {
  WNDCLASSEXW cls;
  WNDCLASSEXW existing;

  existing.cbSize = sizeof(existing);
  if (GetClassInfoExW(WHTT_INSTANCE, WHTT_WINDOW_CLASS, &existing))
    return true;      /* a second call in one process */
  memset(&cls, 0, sizeof(cls));
  cls.cbSize = sizeof(cls);
  cls.lpfnWndProc = loginWindowProc;
  cls.hInstance = WHTT_INSTANCE;
  cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
  cls.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
  cls.lpszClassName = WHTT_WINDOW_CLASS;
  return RegisterClassExW(&cls) != 0;
}

static HWND makeChild(LoginWindow *s, LPCWSTR cls, LPCWSTR text, DWORD style, int id) {
  const HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                                     0, 0, 0, 0, s->frame, (HMENU) (INT_PTR) id,
                                     WHTT_INSTANCE, NULL);

  if (child != NULL && s->font != NULL)
    SendMessageW(child, WM_SETFONT, (WPARAM) s->font, TRUE);
  return child;
}

/* Centres SIZE on OWNER, or on its monitor's work area when there is no owner. */
static void placeOnOwner(HWND owner, int width, int height, int *x, int *y) {
  RECT area;
  RECT over;

  if (owner == NULL || !GetWindowRect(owner, &over)) {
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0)) {
      *x = *y = CW_USEDEFAULT;
      return;
    }
    over = area;
  }
  *x = over.left + ((over.right - over.left) - width) / 2;
  *y = over.top + ((over.bottom - over.top) - height) / 2;
  if (*x < 0)
    *x = 0;
  if (*y < 0)
    *y = 0;
}

static HRESULT startBrowser(const std::shared_ptr<LoginWindow> &state) {
  const std::wstring profile = wideOf(state->request->profile);
  const std::wstring url = wideOf(state->request->url);

  return CreateCoreWebView2EnvironmentWithOptions(NULL,
    profile.empty() ? NULL : profile.c_str(), NULL,
    Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [state, url](HRESULT made, ICoreWebView2Environment *environment) -> HRESULT {
        LoginWindow *const s = state.get();

        /* The user closed the window before the browser was ready. */
        if (s->frame == NULL)
          return S_OK;
        if (FAILED(made) || environment == NULL) {
          s->result = WhttBrowserLoginNoBrowser;
          requestFinish(s);
          return S_OK;
        }
        const HRESULT hr = environment->CreateCoreWebView2Controller(s->frame,
          Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [state, url](HRESULT ready, ICoreWebView2Controller *controller) -> HRESULT {
              LoginWindow *const win = state.get();

              /* The window went while the browser was still starting, so close the
                 controller it did manage to build rather than orphan it. */
              if (win->frame == NULL) {
                if (controller != NULL)
                  controller->Close();
                return S_OK;
              }
              if (FAILED(ready) || controller == NULL
                  || FAILED(controller->get_CoreWebView2(&win->webview)) || !win->webview) {
                win->result = WhttBrowserLoginNoBrowser;
                requestFinish(win);
                return S_OK;
              }
              win->controller = controller;

              ComPtr<ICoreWebView2Settings> settings;
              ComPtr<ICoreWebView2Settings2> settings2;
              EventRegistrationToken token;

              /* The crawl has to send what the login was granted to, so the caller takes
                 the browser's own string rather than guessing at it. */
              if (SUCCEEDED(win->webview->get_Settings(&settings)) && settings
                  && SUCCEEDED(settings.As(&settings2))) {
                LPWSTR agent = NULL;

                if (SUCCEEDED(settings2->get_UserAgent(&agent)))
                  win->userAgent = takeString(agent);
              }
              /* The other half of the Tab bridge in the message loop: without this the
                 focus goes into the page and never comes back out. */
              controller->add_MoveFocusRequested(
                Callback<ICoreWebView2MoveFocusRequestedEventHandler>(
                  [state](ICoreWebView2Controller *,
                          ICoreWebView2MoveFocusRequestedEventArgs *args) -> HRESULT {
                    COREWEBVIEW2_MOVE_FOCUS_REASON reason =
                      COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT;

                    /* The window is already going, so there is nothing to focus and
                       the frame fallback below would be SetFocus(NULL). */
                    if (state->frame == NULL)
                      return S_OK;
                    if (FAILED(args->get_Reason(&reason))
                        || (reason != COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT
                            && reason != COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS))
                      return S_OK;    /* only the two the page tabs out with */
                    args->put_Handled(TRUE);

                    const HWND next = reason == COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS
                                      ? state->done : state->address;

                    /* A control that was never created, or that the harvest greyed
                       out, must not take the focus away for good: SetFocus would leave
                       the thread with none. */
                    SetFocus(next != NULL && IsWindowEnabled(next) ? next : state->frame);
                    return S_OK;
                  }).Get(), &token);
              /* A browser that dies owes an answer nobody will deliver, and the window
                 waits for one, so this is what keeps the program from freezing. */
              win->webview->add_ProcessFailed(
                Callback<ICoreWebView2ProcessFailedEventHandler>(
                  [state](ICoreWebView2 *, ICoreWebView2ProcessFailedEventArgs *) -> HRESULT {
                    if (state->frame == NULL)
                      return S_OK;
                    state->gaveUp = true;
                    requestFinish(state.get());
                    return S_OK;
                  }).Get(), &token);
              win->webview->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
                [state](ICoreWebView2 *sender, ICoreWebView2SourceChangedEventArgs *) -> HRESULT {
                  LPWSTR uri = NULL;

                  if (SUCCEEDED(sender->get_Source(&uri)) && uri != NULL)
                    SetWindowTextW(state->address, uri);
                  if (uri != NULL)
                    CoTaskMemFree(uri);
                  return S_OK;
                }).Get(), &token);
              /* A popup would be a second top-level window over a disabled owner, and
                 nothing of ours would close it, so the login runs in this one. */
              win->webview->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [](ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
                  LPWSTR uri = NULL;

                  args->put_Handled(TRUE);
                  if (SUCCEEDED(args->get_Uri(&uri)) && uri != NULL)
                    sender->Navigate(uri);
                  if (uri != NULL)
                    CoTaskMemFree(uri);
                  return S_OK;
                }).Get(), &token);

              layoutWindow(win);
              controller->put_IsVisible(TRUE);
              if (!url.empty())
                win->webview->Navigate(url.c_str());
              controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
              return S_OK;
            }).Get());

        if (FAILED(hr)) {
          s->result = WhttBrowserLoginNoBrowser;
          requestFinish(s);
        }
        return S_OK;
      }).Get());
}

/* Hands the caller what the browser left behind. Runs with the window already gone, so
   the callback may open a dialog of its own. */
static void reportHarvest(LoginWindow *s) {
  const WhttBrowserLoginRequest *const request = s->request;

  if (request->userAgent != NULL && request->userAgentSize != 0)
    strncpy_s(request->userAgent, request->userAgentSize, s->userAgent.c_str(), _TRUNCATE);
  if (request->onCookie == NULL)
    return;

  /* Taken away first, because a second answer from the browser would grow the vector
     under the loop and move every string the caller is reading. */
  std::vector<HarvestedCookie> harvested;

  harvested.swap(s->cookies);
  for(size_t i = 0 ; i < harvested.size() ; i++) {
    const HarvestedCookie &taken = harvested[i];
    WhttBrowserLoginCookie one;

    one.name = taken.name.c_str();
    one.value = taken.value.c_str();
    one.domain = taken.domain.c_str();
    one.path = taken.path.c_str();
    one.secure = taken.secure;
    one.expires = taken.expires;
    if (!request->onCookie(request->onCookieCtx, &one))
      break;
  }
}

static int runLoginWindow(const std::shared_ptr<LoginWindow> &state) {
  LoginWindow *const s = state.get();
  NONCLIENTMETRICSW metrics;
  HWND owner = (HWND) s->request->parent;
  const std::wstring title = wideOf(s->request->title);
  int width, height, x, y;
  MSG message;

  if (owner != NULL && !IsWindow(owner))
    owner = NULL;
  if (!registerWindowClass())
    return WhttBrowserLoginNoWindow;

  memset(&metrics, 0, sizeof(metrics));
  metrics.cbSize = sizeof(metrics);
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    s->font = CreateFontIndirectW(&metrics.lfMessageFont);

  const HDC screen = GetDC(NULL);

  s->dpi = screen != NULL ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen != NULL)
    ReleaseDC(NULL, screen);

  RECT work;

  width = scaled(s, 1024);
  height = scaled(s, 760);
  if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
    if (width > work.right - work.left)
      width = work.right - work.left;
    if (height > work.bottom - work.top)
      height = work.bottom - work.top;
  }
  placeOnOwner(owner, width, height, &x, &y);
  s->frame = CreateWindowExW(0, WHTT_WINDOW_CLASS,
                             title.empty() ? L"Log in with a browser" : title.c_str(),
                             WS_OVERLAPPEDWINDOW, x, y, width, height,
                             owner, NULL, WHTT_INSTANCE, s);
  if (s->frame == NULL) {
    if (s->font != NULL)
      DeleteObject(s->font);
    return WhttBrowserLoginNoWindow;
  }
  s->address = makeChild(s, L"EDIT", wideOf(s->request->url).c_str(),
                         WS_BORDER | ES_AUTOHSCROLL, WHTT_ID_ADDRESS);
  s->go = makeChild(s, L"BUTTON", L"Go", BS_PUSHBUTTON, WHTT_ID_GO);
  s->done = makeChild(s, L"BUTTON", L"Done", BS_PUSHBUTTON, WHTT_ID_DONE);
  layoutWindow(s);

  /* Modal by hand, because the owner's own message loop is the one we are standing in. */
  if (owner != NULL && IsWindowEnabled(owner)) {
    EnableWindow(owner, FALSE);
    s->disabledParent = owner;
  }
  ShowWindow(s->frame, SW_SHOW);
  UpdateWindow(s->frame);
  if (FAILED(startBrowser(state))) {
    s->result = WhttBrowserLoginNoBrowser;
    finishWindow(s);
  }

  while (!s->finished) {
    const BOOL got = GetMessageW(&message, NULL, 0, 0);

    if (got == 0) {
      PostQuitMessage((int) message.wParam);   /* the application is closing, not us */
      break;
    }
    if (got < 0)
      break;
    const HWND focus = GetFocus();

    /* Tab off the end of our own controls is what puts the focus in the page, because
       the dialog manager only ever cycles the three of them. */
    if (s->controller && message.message == WM_KEYDOWN && message.wParam == VK_TAB) {
      const BOOL back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

      if ((!back && focus == s->done) || (back && focus == s->address)) {
        s->controller->MoveFocus(back ? COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS
                                      : COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT);
        continue;
      }
    }
    /* Tab and Enter belong to the page whenever the page holds the focus. Asking for
       the focused window beats tracking it, because the browser reports a focus change
       from another process and the answer would arrive a keystroke late. The frame
       counts as ours, because it holds the focus until the browser has started. */
    if (s->frame != NULL && (focus == NULL || focus == s->frame || ourControl(s, focus))
        && IsDialogMessageW(s->frame, &message))
      continue;
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  /* The loop can break without having torn anything down, when the application posts
     WM_QUIT or GetMessage fails, and the browser must not outlive the window it is
     parented to. finishWindow() does nothing when the teardown already ran. */
  finishWindow(s);
  if (owner != NULL)
    SetActiveWindow(owner);
  if (s->font != NULL)
    DeleteObject(s->font);
  /* Decided here rather than where we gave up, because closing the browser can deliver
     the answer we had stopped waiting for. Whether the answer came, not whether it
     carried anything: an empty jar is still the browser having answered. */
  if (s->result == WhttBrowserLoginDone && s->gaveUp && !s->answered)
    s->result = WhttBrowserLoginBrowserGone;
  return s->result;
}

int __cdecl WhttBrowserLoginRun(const WhttBrowserLoginRequest *request) {
  if (request == NULL || request->size != sizeof(*request))
    return WhttBrowserLoginBadRequest;
  if (request->userAgent != NULL && request->userAgentSize != 0)
    request->userAgent[0] = '\0';
  if (!WhttBrowserLoginAvailable(NULL, 0))
    return WhttBrowserLoginNoRuntime;

  /* The caller is an MFC application that has already done this, so S_FALSE is the
     expected answer and the matching CoUninitialize() is still ours to make. */
  const HRESULT apartment = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

  if (apartment == RPC_E_CHANGED_MODE)
    return WhttBrowserLoginNoApartment;

  const std::shared_ptr<LoginWindow> state = std::make_shared<LoginWindow>();

  state->request = request;

  const int result = runLoginWindow(state);

  if (result == WhttBrowserLoginDone)
    reportHarvest(state.get());
  if (SUCCEEDED(apartment))
    CoUninitialize();
  return result;
}
