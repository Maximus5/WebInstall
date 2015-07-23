
/*
Copyright (c) 2015 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define HIDE_USE_EXCEPTION_INFO
#define SHOWDEBUGSTR

#include "../common/common.hpp"
#include "../common/WObjects.h"
#include "../common/StartupEnvEx.h"
#include "../ConEmu/DpiAware.h"
#include "../ConEmu/DynDialog.h"
#include "../ConEmuC/Downloader.h"

#include <tchar.h>
#include <Commctrl.h>

#include "resource.h"

#define DEBUGSTRINET(s) //DEBUGSTR(s)
#define DEBUGSTRSTARTUP(s) //DEBUGSTR(s)

HINSTANCE g_hInstance = NULL;
OSVERSIONINFO gOSVer = {};
WORD gnOsVer = 0x500;
CEStartupEnv* gpStartEnv = NULL;
BOOL gbInDisplayLastError = FALSE;
CDpiForDialog* gp_DpiAware = NULL;
HWND gh_Dlg = NULL;
HMONITOR gh_StartMon = NULL;
STARTUPINFO g_SI = { sizeof(g_SI) };

enum InstallSteps {
	st_DownloadIni = 0,
	st_ParseIni,
	st_DownloadInstaller,
	st_RunInstaller,
	st_Finish
};
LPCWSTR InstallStepsNames[] = {
	L"Downloading latest version information",
	L"Parsing version information",
	L"Downloading installer",
	L"Running installer",
	L"All done"
};
InstallSteps g_Step = st_DownloadIni;

LPCWSTR VersionLocations[] = {
	L"http://conemu.github.io/version.ini",
	L"http://conemu-maximus5.googlecode.com/svn/trunk/ConEmu/version.ini",
	L"http://www.conemu.ru/version.ini",
	NULL
};
wchar_t gs_ProcessingPath[MAX_PATH] = L"";

void UpdateShowStep()
{
	SetDlgItemText(gh_Dlg, tOperation, InstallStepsNames[g_Step]);
	SetDlgItemText(gh_Dlg, tURL, gs_ProcessingPath);
}


void LogString(LPCWSTR asInfo, bool abWriteTime /*= true*/, bool abWriteLine /*= true*/)
{
	//if (gpLog) gpLog->LogString(asInfo, abWriteTime, abWriteLine);
}

int MsgBox(LPCTSTR lpText, UINT uType, LPCTSTR lpCaption = NULL, HWND ahParent = NULL, bool abModal = true)
{
	//DontEnable de(abModal);

	//ghDlgPendingFrom = GetForegroundWindow();

	HWND hParent = ahParent;
		//gbMessagingStarted
		//? ((ahParent == (HWND)-1) ? ghWnd : ahParent)
		//: NULL;

	//HooksUnlocker;

	int nBtn = MessageBox(hParent, lpText ? lpText : L"<NULL>", lpCaption ? lpCaption : L"ConEmu WebInstaller", uType);

	//ghDlgPendingFrom = NULL;

	return nBtn;
}

int DisplayLastError(LPCTSTR asLabel, DWORD dwError /* =0 */, DWORD dwMsgFlags /* =0 */, LPCWSTR asTitle /*= NULL*/, HWND hParent /*= NULL*/)
{
	int nBtn = 0;
	DWORD dw = dwError ? dwError : GetLastError();
	wchar_t* lpMsgBuf = NULL;
	wchar_t *out = NULL;

	if (dw && (dw != (DWORD)-1))
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);
		INT_PTR nLen = _tcslen(asLabel) + 64 + (lpMsgBuf ? _tcslen(lpMsgBuf) : 0);
		out = new wchar_t[nLen];
		_wsprintf(out, SKIPLEN(nLen) _T("%s\nLastError=0x%08X\n%s"), asLabel, dw, lpMsgBuf);
	}

	//if (gbMessagingStarted) apiSetForegroundWindow(hParent ? hParent : ghWnd);

	if (!dwMsgFlags) dwMsgFlags = MB_SYSTEMMODAL | MB_ICONERROR;

	BOOL lb = gbInDisplayLastError; gbInDisplayLastError = TRUE;
	nBtn = MsgBox(out ? out : asLabel, dwMsgFlags, asTitle, hParent);
	gbInDisplayLastError = lb;

	if (lpMsgBuf)
		LocalFree(lpMsgBuf);
	if (out)
		delete[] out;
	return nBtn;
}

void EditIconHint_ResChanged(HWND) {};

int ProcessCommandLine(LPCWSTR asCommandLine)
{
	return 0;
}

BOOL MoveWindowRect(HWND hWnd, const RECT& rcWnd, BOOL bRepaint = FALSE)
{
	return MoveWindow(hWnd, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, bRepaint);
}

INT_PTR WINAPI downloadDlgProc(HWND hDlg, UINT messg, WPARAM wParam, LPARAM lParam)
{
	INT_PTR lRc = 0;

	switch (messg)
	{
	case WM_INITDIALOG:
	{
		gh_Dlg = hDlg;

		if (gp_DpiAware)
		{
			gp_DpiAware->Attach(hDlg, NULL, CDynDialog::GetDlgClass(hDlg));
		}

		UpdateShowStep();

		RECT rect = {};
		if (GetWindowRect(hDlg, &rect))
		{
			CDpiAware::GetCenteredRect(NULL, rect, gh_StartMon);
			MoveWindowRect(hDlg, rect);
		}

		SetFocus(GetDlgItem(hDlg, IDCANCEL));

		return FALSE;
	}

	case WM_COMMAND:
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
			switch (LOWORD(wParam))
			{
			case IDOK:
			case IDCANCEL:
			case IDCLOSE:
				downloadDlgProc(hDlg, WM_CLOSE, 0, 0);
				return 1;
			} // BN_CLICKED
			break;
		} // switch (HIWORD(wParam))
		break;

	case WM_CLOSE:
		if (gp_DpiAware)
			gp_DpiAware->Detach();
		EndDialog(hDlg, IDOK);
		break;

	case WM_DESTROY:
		break;

	default:
		if (gp_DpiAware && gp_DpiAware->ProcessDpiMessages(hDlg, messg, wParam, lParam))
		{
			return TRUE;
		}
	}

	return FALSE;
}

int DoDownload()
{
	gp_DpiAware = new CDpiForDialog();
	
	INT_PTR iRc = CDynDialog::ExecuteDialog(IDD_PROGRESS, NULL, downloadDlgProc, 0);

	delete gp_DpiAware;
	gp_DpiAware = NULL;
	return 0;
}

int DoInstall()
{
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	DEBUGSTRSTARTUP(L"WinMain entered");
	int iMainRc = 0, iFRc;

	g_hInstance = hInstance;
	ZeroStruct(gOSVer);
	gOSVer.dwOSVersionInfoSize = sizeof(gOSVer);
	GetVersionEx(&gOSVer);
	gnOsVer = ((gOSVer.dwMajorVersion & 0xFF) << 8) | (gOSVer.dwMinorVersion & 0xFF);
	HeapInitialize();
	GetStartupInfo(&g_SI);

	MONITORINFO mi = {sizeof(mi)};
	if (g_SI.hStdOutput && GetMonitorInfoW((HMONITOR)g_SI.hStdOutput, &mi))
		gh_StartMon = (HMONITOR)g_SI.hStdOutput;

	INITCOMMONCONTROLSEX init = {sizeof(init), ICC_PROGRESS_CLASS};
	InitCommonControlsEx(&init);

	// On Vista and higher ensure our process will be
	// marked as fully dpi-aware, regardless of manifest
	if (gnOsVer >= 0x600)
	{
		CDpiAware::setProcessDPIAwareness();
	}

	/* *** DEBUG PURPOSES */
	gpStartEnv = LoadStartupEnvEx::Create();
	if (gnOsVer >= 0x600)
	{
		CDpiAware::UpdateStartupInfo(gpStartEnv);
	}
	/* *** DEBUG PURPOSES */

	if ((iFRc = ProcessCommandLine(GetCommandLineW())) != 0)
	{
		iMainRc = iFRc;
		goto wrap;
	}

	if ((iFRc = DoDownload()) != 0)
	{
		iMainRc = iFRc;
		goto wrap;
	}

	if ((iFRc = DoInstall()) != 0)
	{
		iMainRc = iFRc;
		goto wrap;
	}
wrap:
	DEBUGSTRSTARTUP(L"WinMain exit");
	return iMainRc;
}
