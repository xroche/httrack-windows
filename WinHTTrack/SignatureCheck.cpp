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
/*       Authenticode check of our own binaries                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "stdafx.h"

#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <process.h>

#include <stdio.h>
#include <string.h>

#include <WS2tcpip.h>  // Note: weird C2894 error if not included here
extern "C" {
  #include "HTTrackInterface.h"
};

#include "SignatureCheck.h"

/* Certum prefixes every open-source certificate with that wording. Matched whole and
   never as a fragment: "Roche Xavier" is a name anybody can buy a certificate for. */
static const char *const WhttSigOurNames[] = {
  "Open Source Developer Roche Xavier"
};

/* Checked beside us when present -- a ZIP install is whatever the user unpacked. The
   app-local Visual C++ runtime is left out: it stays signed by Microsoft, not by us. */
static const char *const WhttSigSiblings[] = {
  "libhttrack.dll", "httrack.exe"
};

static volatile LONG WhttSigDone = 0;
static volatile LONG WhttSigWarned = 0;
static WhttSigVerdict WhttSigWorstVerdict = WHTT_SIG_OURS;
static char WhttSigLine[512] = "";

/* The signer's common name, read straight from the file. Filled in even when the chain
   does not check out, where it is a claim and not an identity: never use it alone. */
static BOOL WhttSigSignerName(const WCHAR *wpath, char *out, size_t size) {
  HCERTSTORE store = NULL;
  HCRYPTMSG msg = NULL;
  CMSG_SIGNER_INFO *signer = NULL;
  DWORD needed = 0;
  BOOL ok = FALSE;

  if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, wpath,
                        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                        CERT_QUERY_FORMAT_FLAG_BINARY, 0, NULL, NULL, NULL,
                        &store, &msg, NULL)) {
    return FALSE;
  }
  if (CryptMsgGetParam(msg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &needed)
      && needed != 0
      && (signer = (CMSG_SIGNER_INFO *) malloct(needed)) != NULL
      && CryptMsgGetParam(msg, CMSG_SIGNER_INFO_PARAM, 0, signer, &needed)) {
    CERT_INFO id;
    PCCERT_CONTEXT cert;

    memset(&id, 0, sizeof(id));
    id.Issuer = signer->Issuer;
    id.SerialNumber = signer->SerialNumber;
    cert = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                      CERT_FIND_SUBJECT_CERT, &id, NULL);
    if (cert != NULL) {
      /* The count includes the terminator, so an empty name comes back as 1. */
      ok = CertGetNameStringA(cert, CERT_NAME_ATTR_TYPE, 0, (void *) szOID_COMMON_NAME,
                              out, (DWORD) size) > 1;
      CertFreeCertificateContext(cert);
    }
  }
  freet(signer);
  if (msg != NULL) {
    CryptMsgClose(msg);
  }
  if (store != NULL) {
    CertCloseStore(store, 0);
  }
  return ok;
}

WhttSigVerdict WhttSigVerifyFile(const char *path, char *publisher,
                                 size_t publisherSize, LONG *status) {
  GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  WINTRUST_FILE_INFO file;
  WINTRUST_DATA data;
  WCHAR wpath[MAX_PATH * 2];
  char name[256];
  BOOL named;
  LONG res;
  size_t i;

  if (publisher != NULL && publisherSize != 0) {
    *publisher = '\0';
  }
  if (status != NULL) {
    *status = (LONG) ERROR_INVALID_PARAMETER;
  }
  if (MultiByteToWideChar(CP_ACP, 0, path, -1, wpath,
                          (int) (sizeof(wpath) / sizeof(wpath[0]))) == 0) {
    return WHTT_SIG_UNKNOWN;
  }

  memset(&file, 0, sizeof(file));
  file.cbStruct = sizeof(file);
  file.pcwszFilePath = wpath;

  memset(&data, 0, sizeof(data));
  data.cbStruct = sizeof(data);
  data.dwUIChoice = WTD_UI_NONE;
  /* Neither revocation nor a chain fetch: the privacy policy promises this program
     opens no connection of its own, and a lookup would stall an offline machine.
     WTD_LIFETIME_SIGNING_FLAG stays off, or every signature would go bad the day the
     certificate expires instead of living on through its timestamp. */
  data.fdwRevocationChecks = WTD_REVOKE_NONE;
  data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  data.dwUnionChoice = WTD_CHOICE_FILE;
  data.pFile = &file;
  data.dwStateAction = WTD_STATEACTION_VERIFY;

  res = WinVerifyTrust((HWND) INVALID_HANDLE_VALUE, &action, &data);
  data.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust((HWND) INVALID_HANDLE_VALUE, &action, &data);
  if (status != NULL) {
    *status = res;
  }

  name[0] = '\0';
  named = WhttSigSignerName(wpath, name, sizeof(name));
  if (named && publisher != NULL && publisherSize != 0) {
    strncpy_s(publisher, publisherSize, name, _TRUNCATE);
  }

  switch (res) {
  case ERROR_SUCCESS:
    for (i = 0; i < sizeof(WhttSigOurNames) / sizeof(WhttSigOurNames[0]); i++) {
      if (_stricmp(name, WhttSigOurNames[i]) == 0) {
        return WHTT_SIG_OURS;
      }
    }
    return WHTT_SIG_OTHERS;
  case TRUST_E_BAD_DIGEST:
    return WHTT_SIG_TAMPERED;
  case TRUST_E_NOSIGNATURE:
  case TRUST_E_SUBJECT_FORM_UNKNOWN:
  case TRUST_E_PROVIDER_UNKNOWN:
    return named ? WHTT_SIG_UNKNOWN : WHTT_SIG_NONE;
  default:
    /* An untrusted root, an unknown digest algorithm and an expired chain all land
       here, and so do our own binaries on an un-updated Windows 7. So does a corrupt
       certificate (TRUST_E_CERT_SIGNATURE), which a system too old for the signing
       algorithm cannot be told apart from. Never an accusation. */
    return WHTT_SIG_UNKNOWN;
  }
}

/* How loudly a verdict speaks: an install is described by its worst file. */
static int WhttSigSeverity(WhttSigVerdict verdict) {
  switch (verdict) {
  case WHTT_SIG_TAMPERED: return 4;
  case WHTT_SIG_OTHERS:   return 3;
  case WHTT_SIG_UNKNOWN:  return 2;
  case WHTT_SIG_NONE:     return 1;
  default:                return 0;
  }
}

static const char *WhttSigWord(WhttSigVerdict verdict) {
  switch (verdict) {
  case WHTT_SIG_OURS:     return "verified";
  case WHTT_SIG_OTHERS:   return "OTHER PUBLISHER";
  case WHTT_SIG_TAMPERED: return "MODIFIED since it was signed";
  case WHTT_SIG_NONE:     return "unsigned";
  default:                return "unverifiable on this system";
  }
}

/* The directory this program was loaded from, with its trailing backslash. Everything
   is anchored here and never on the current directory or the registry, which is what
   lets the ZIP package work wherever it is unpacked. */
static BOOL WhttSigModuleDir(char *out, size_t size) {
  const DWORD n = GetModuleFileNameA(NULL, out, (DWORD) size);
  char *sep;

  if (n == 0 || n >= size) {
    return FALSE;
  }
  sep = strrchr(out, '\\');
  if (sep == NULL) {
    return FALSE;
  }
  sep[1] = '\0';
  return TRUE;
}

static unsigned __stdcall WhttSigThread(void *arg) {
  HWND notify = (HWND) arg;
  char dir[MAX_PATH];
  char path[MAX_PATH];
  char publisher[256];
  char worstFile[MAX_PATH];
  char worstWho[256];
  char ourWho[256];
  WhttSigVerdict worst = WHTT_SIG_UNKNOWN;
  size_t i;

  worstFile[0] = worstWho[0] = ourWho[0] = '\0';
  if (GetModuleFileNameA(NULL, path, sizeof(path)) != 0) {
    const WhttSigVerdict v = WhttSigVerifyFile(path, publisher, sizeof(publisher), NULL);
    const char *const leaf = strrchr(path, '\\');

    strncpy_s(ourWho, sizeof(ourWho), publisher, _TRUNCATE);
    worst = v;
    strncpy_s(worstFile, sizeof(worstFile), leaf != NULL ? leaf + 1 : path, _TRUNCATE);
    strncpy_s(worstWho, sizeof(worstWho), publisher, _TRUNCATE);
  }
  if (WhttSigModuleDir(dir, sizeof(dir))) {
    for (i = 0; i < sizeof(WhttSigSiblings) / sizeof(WhttSigSiblings[0]); i++) {
      WhttSigVerdict v;

      _snprintf_s(path, sizeof(path), _TRUNCATE, "%s%s", dir, WhttSigSiblings[i]);
      /* Absent is not evidence of anything: a ZIP user may simply not have kept it. */
      if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        continue;
      }
      v = WhttSigVerifyFile(path, publisher, sizeof(publisher), NULL);
      if (WhttSigSeverity(v) > WhttSigSeverity(worst)) {
        worst = v;
        strncpy_s(worstFile, sizeof(worstFile), WhttSigSiblings[i], _TRUNCATE);
        strncpy_s(worstWho, sizeof(worstWho), publisher, _TRUNCATE);
      }
    }
  }

  switch (worst) {
  case WHTT_SIG_TAMPERED:
    _snprintf_s(WhttSigLine, sizeof(WhttSigLine), _TRUNCATE,
                "%s has been modified since it was signed.", worstFile);
    break;
  case WHTT_SIG_OTHERS:
    _snprintf_s(WhttSigLine, sizeof(WhttSigLine), _TRUNCATE,
                "Published by \"%s\", not by the HTTrack project.", worstWho);
    break;
  case WHTT_SIG_UNKNOWN:
    if (worstWho[0] != '\0') {
      _snprintf_s(WhttSigLine, sizeof(WhttSigLine), _TRUNCATE,
                  "Publisher: %s (this system cannot check the certificate).", worstWho);
    } else {
      strncpy_s(WhttSigLine, sizeof(WhttSigLine),
                "The signature could not be checked on this system.", _TRUNCATE);
    }
    break;
  case WHTT_SIG_NONE:
    strncpy_s(WhttSigLine, sizeof(WhttSigLine),
              "Not signed: built from source, not an official release.", _TRUNCATE);
    break;
  default:
    _snprintf_s(WhttSigLine, sizeof(WhttSigLine), _TRUNCATE,
                "Verified publisher: %s", ourWho);
    break;
  }
  WhttSigWorstVerdict = worst;
  /* Publishes both of the above: Interlocked is a full barrier, so whoever sees the
     flag set sees the results that were written before it. */
  InterlockedExchange(&WhttSigDone, 1);
  /* Nothing to handle: it exists to end the wait in GetMessage, so that OnIdle runs
     again and finds the verdict even if the user never touches anything. */
  if (notify != NULL) {
    PostMessage(notify, WM_NULL, 0, 0);
  }
  return 0;
}

void WhttSigCheckStart(HWND notify) {
  unsigned id = 0;
  const uintptr_t thread = _beginthreadex(NULL, 0, WhttSigThread, (void *) notify, 0,
                                          &id);

  /* A thread that never starts leaves the verdict unset, which shows nothing and warns
     about nothing -- the right way for this to fail. */
  if (thread != 0) {
    CloseHandle((HANDLE) thread);
  }
}

WhttSigVerdict WhttSigOverall(void) {
  return InterlockedCompareExchange(&WhttSigDone, 0, 0) != 0 ? WhttSigWorstVerdict
                                                             : WHTT_SIG_OURS;
}

BOOL WhttSigIsUnofficial(void) {
  const WhttSigVerdict worst = WhttSigOverall();

  return worst == WHTT_SIG_TAMPERED || worst == WHTT_SIG_OTHERS;
}

const char *WhttSigSummary(void) {
  return InterlockedCompareExchange(&WhttSigDone, 0, 0) != 0 ? WhttSigLine : "";
}

BOOL WhttSigTakeWarning(void) {
  if (!WhttSigIsUnofficial()) {
    return FALSE;
  }
  return InterlockedExchange(&WhttSigWarned, 1) == 0;
}

/* The raw status is printed on purpose: it is the only thing that tells a Windows 7
   missing our root apart from one missing SHA-2 altogether, and both read the same to
   the user. */
static void WhttSigReportOne(FILE *out, const char *path) {
  char publisher[256];
  LONG status = 0;
  const char *const leaf = strrchr(path, '\\');
  WhttSigVerdict verdict;

  /* A path that cannot be opened answers TRUST_E_NOSIGNATURE like a genuinely unsigned
     file does, so say which it was: a mistyped path otherwise reads as a verdict. */
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
    fprintf(out, "%s: cannot be opened (0x%08lX)\n", leaf != NULL ? leaf + 1 : path,
            (unsigned long) HRESULT_FROM_WIN32(GetLastError()));
    return;
  }
  verdict = WhttSigVerifyFile(path, publisher, sizeof(publisher), &status);
  fprintf(out, "%s: %s", leaf != NULL ? leaf + 1 : path, WhttSigWord(verdict));
  if (publisher[0] != '\0') {
    fprintf(out, " [%s]", publisher);
  }
  fprintf(out, " (0x%08lX)\n", (unsigned long) status);
}

void WhttSigReport(FILE *out, const char *path) {
  char dir[MAX_PATH];
  char self[MAX_PATH];
  size_t i;

  if (path != NULL) {
    WhttSigReportOne(out, path);
  } else {
    if (GetModuleFileNameA(NULL, self, sizeof(self)) != 0) {
      WhttSigReportOne(out, self);
    }
    if (WhttSigModuleDir(dir, sizeof(dir))) {
      for (i = 0; i < sizeof(WhttSigSiblings) / sizeof(WhttSigSiblings[0]); i++) {
        char sibling[MAX_PATH];

        _snprintf_s(sibling, sizeof(sibling), _TRUNCATE, "%s%s", dir, WhttSigSiblings[i]);
        if (GetFileAttributesA(sibling) != INVALID_FILE_ATTRIBUTES) {
          WhttSigReportOne(out, sibling);
        }
      }
    }
  }
  /* The caller leaves through ExitProcess(), which does not flush the CRT. */
  fflush(out);
}
