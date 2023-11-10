// main.c

#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include "resource.h"

#pragma intrinsic(strcpy)

#pragma function(memset)
void* __cdecl memset(void* dst, int val, size_t count) {
    void* start = dst;
    while (count--) {
        *(char*)dst = (char)val;
        dst = (char*)dst + 1;
    }
    return start;
}

static HICON g_hIcon = NULL;
static HINSTANCE g_hInst = NULL;
static HWND g_hDlg = NULL;
static HWND g_hTip1 = NULL;
static char g_path[1024];
static char g_tabs = 0;

static const char* szTip1 = "Amount of shared stash tabs";
static const BYTE stash_bytes[] = {
  0x55,0xAA,0x55,0xAA,0x00,0x00,0x00,0x00,0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x4A,0x4D,0x00,0x00
};

static BYTE* find_pattern(const BYTE* src_start, const BYTE* src_end, const BYTE* pattern_start, const BYTE* pattern_end) {
  const BYTE *pos,*end,*s1,*p1;
  end = src_end-(pattern_end-pattern_start);
  for (pos = src_start; pos <= end; pos++) {
    s1 = pos-1;
    p1 = pattern_start-1;
    while (*++s1 == *++p1) {
      if (p1 == pattern_end)
        return (BYTE*)pos;
    }
  }
  return (BYTE*)src_end;
}

static HWND CreateToolTip(HWND hDlg, int ctrlID, const char* lpszText)
{
  HWND hWndCtrl, hWndTip;
  TTTOOLINFOA toolInfo;

  hWndCtrl = GetDlgItem(hDlg, ctrlID);
  if (hWndCtrl) {
    hWndTip = CreateWindowExA(
      0, "tooltips_class32", NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOANIMATE | TTS_NOFADE,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hDlg, NULL, g_hInst, NULL
    );

    if (hWndTip) {
      memset(&toolInfo, 0, sizeof(TTTOOLINFOA));
      toolInfo.cbSize = sizeof(TTTOOLINFOA);
      toolInfo.hwnd = hDlg;
      toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
      toolInfo.uId = (UINT_PTR)hWndCtrl;
      toolInfo.lpszText = (char*)lpszText;
      SendMessageA(hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
      return hWndTip;
    }
  }
  return NULL;
}

static HWND SetToolTip(HWND hDlg, int ctrlID, HWND* phWndTip, const char* lpszText) {
  HWND hWndCtrl;

  if (*phWndTip) {
    hWndCtrl = GetDlgItem(hDlg, ctrlID);
    if (hWndCtrl) {
      TTTOOLINFOA toolInfo;
      char buf[1024];
      memset(&toolInfo, 0, sizeof(TTTOOLINFOA));
      toolInfo.cbSize = sizeof(TTTOOLINFOA);
      toolInfo.hwnd = hDlg;
      toolInfo.uId = (UINT_PTR)hWndCtrl;
      toolInfo.lpszText = buf;
      SendMessageA(*phWndTip, TTM_GETTOOLINFO, 0, (LPARAM)&toolInfo);
      strcpy(buf, lpszText);
      SendMessageA(*phWndTip, TTM_SETTOOLINFO, 0, (LPARAM)&toolInfo);
    }
  } else {
    *phWndTip = CreateToolTip(hDlg, ctrlID, lpszText);
  }
  return *phWndTip;
}

static int AddTabs(unsigned int c) {
  HANDLE hFile;
  DWORD dwBytesWritten = 0;
  unsigned int i;
  hFile = CreateFileA(g_path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile && hFile != INVALID_HANDLE_VALUE) {
    for (i = 0; i < c; i++) {
      WriteFile(hFile, stash_bytes, 68, &dwBytesWritten, NULL);
    }
    CloseHandle(hFile);
  }
  return (dwBytesWritten == 68);
}

static char CountTabs(void) {
  char ret = -1;
  HANDLE hFile = CreateFileA(g_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if(hFile && hFile != INVALID_HANDLE_VALUE) {
    HANDLE hMap = CreateFileMappingA(hFile, 0, PAGE_READONLY, 0, 0, NULL);
    if(hMap && hMap != INVALID_HANDLE_VALUE) {
      BYTE* pView = (BYTE*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
      if (pView) {
        MEMORY_BASIC_INFORMATION memBI;
        memset(&memBI, 0, sizeof(memBI));
        if (VirtualQuery((void*)pView, &memBI, sizeof(memBI))) {
          BYTE* p = pView;
          while (p < pView+memBI.RegionSize) {
            p = find_pattern(p, pView+memBI.RegionSize, stash_bytes, stash_bytes+3) + 68;
            ret++;
          }
        }
        UnmapViewOfFile((void*)pView);
      }
      CloseHandle(hMap);
    }
    CloseHandle(hFile);
  }
  return ret;
}

static void ShowStashPath(void) {
  char *p = g_path, *p2 = g_path;
  while (*++p) if (*p == '\\') p2 = p;
  SendMessageA(GetDlgItem(g_hDlg, IDC_TEXT1), WM_SETTEXT, 0, (LPARAM)p2);
  SetToolTip(g_hDlg, IDC_TEXT1, &g_hTip1, g_path);
}

static void ShowTabCount() {
  g_tabs = CountTabs();
  EnableWindow(GetDlgItem(g_hDlg, IDOK), 0);
  if (g_tabs >= 3 && g_tabs <= 7) {
    SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_SETRANGE, 0, MAKELPARAM(7,g_tabs));
    SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_SETPOS, 0, g_tabs);
    EnableWindow(GetDlgItem(g_hDlg, IDC_SPIN1), (g_tabs < 7));
  } else {
    SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_SETRANGE, 0, MAKELPARAM(0,0));
    SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_SETPOS, 0, 0);
    SendMessageA(GetDlgItem(g_hDlg, IDC_TEXT2), WM_SETTEXT, 0, (LPARAM)"?");
    EnableWindow(GetDlgItem(g_hDlg, IDC_SPIN1), 0);
  }
}

static void Save(void) {
  WORD c = LOWORD(SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_GETPOS, 0, 0));
  if (c > g_tabs && c >= 3 && c <= 7) {
    if (AddTabs(c-g_tabs)) MessageBox(g_hDlg, "Edit successful !", "Success", MB_ICONINFORMATION);
    else MessageBox(g_hDlg, "Edit error !", "Error", MB_ICONERROR);
  }
}

static void HandleDrop(HDROP hDrop) {
  g_path[0] = 0;
  g_tabs = 0;
  DragQueryFileA(hDrop, 0, g_path, 1023);
  DragFinish(hDrop);
  ShowStashPath();
  ShowTabCount();
}

WNDPROC oWndProc;
long __stdcall WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == WM_DROPFILES) {
    HandleDrop((HDROP)wParam);
    return TRUE;
  }

  return CallWindowProcA(oWndProc, hWnd, uMsg, wParam, lParam);
}

WNDPROC oWndProc2;
long __stdcall WndProc2(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg != UDM_GETPOS) {
    WORD c = LOWORD(SendMessageA(GetDlgItem(g_hDlg, IDC_SPIN1), UDM_GETPOS, 0, 0));
    EnableWindow(GetDlgItem(g_hDlg, IDOK), (c > g_tabs && c >= 3 && c <= 7 && g_tabs >= 3 && g_tabs <= 7));
  }

  return CallWindowProcA(oWndProc2, hWnd, uMsg, wParam, lParam);
}

long __stdcall DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch(uMsg) {
    case WM_INITDIALOG:
      g_hDlg = hDlg;
      SendMessageA(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);
      SendMessageA(hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon);
      SendMessageA(GetDlgItem(hDlg, IDC_SPIN1), UDM_SETRANGE, 0, MAKELPARAM(0,0)); // MAKELPARAM(7,CUR)
      SendMessageA(GetDlgItem(hDlg, IDC_SPIN1), UDM_SETPOS, 0, 0);
      SendMessageA(GetDlgItem(hDlg, IDC_TEXT2), WM_SETTEXT, 0, (LPARAM)"?");
      CreateToolTip(hDlg, IDC_TEXT2, szTip1);
      oWndProc = (WNDPROC)SetWindowLongA(GetDlgItem(hDlg, IDC_TEXT1), GWL_WNDPROC, (LONG)WndProc);
      oWndProc2 = (WNDPROC)SetWindowLongA(GetDlgItem(hDlg, IDC_SPIN1), GWL_WNDPROC, (LONG)WndProc2);
      if (g_path[0]) {
        ShowStashPath();
        ShowTabCount();
      }
      return TRUE;

    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDOK:
          EnableWindow(GetDlgItem(g_hDlg, IDOK), 0);
          EnableWindow(GetDlgItem(g_hDlg, IDC_SPIN1), 0);
          Save();
          ShowTabCount(hDlg);
          return TRUE;
      }
      break;

    case WM_DROPFILES:
      HandleDrop((HDROP)wParam);
      return TRUE;

    case WM_CLOSE:
      DestroyWindow(hDlg);
      return TRUE;

    case WM_DESTROY:
      PostQuitMessage(0);
      return TRUE;
  }

  return FALSE;
}

int __stdcall start(void)
{
  HWND hDlg;
  MSG msg;
  BOOL ret;
  char *s;
  unsigned int i;

  g_path[0] = 0;
  s = GetCommandLineA();
  if (s && *s) {
    if (*s == '"') {
      s++;
      while (*s)
        if (*s++ == '"')
          break;
    } else {
      while (*s && *s != ' ' && *s != '\t')
        s++;
    }
    while (*s == L' ' || *s == L'\t')
      s++;

    while (*s == '"') s++;
    if (*s) {
      for (i = 0; i < 1023; i++) {
        if (*s == 0 || *s == '"') break;
        g_path[i] = *s++;
      }

      g_path[i] = 0;
    }
  }

  g_hInst = GetModuleHandleA(0);
  g_hIcon = LoadIconA(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
  hDlg = CreateDialogParamA(g_hInst, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc, 0);
  ShowWindow(hDlg, SW_SHOWNORMAL);

  while((ret = GetMessageA(&msg, 0, 0, 0)) != 0) {
    if(ret == -1)
      return -1;

    if(!IsDialogMessageA(hDlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  }

  return 0;
}
