//
//	DisplayProcessInfo.c
//  Copyright (c) 2002 by J Brown 
//	Freeware
//
//	void SetProcesInfo(HWND hwnd)
//
//	Fill the process-tab-pane with proces info for the
//  specified window. 
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "WinSpy.h"
#include "resource.h"

#include <tlhelp32.h>


typedef BOOL(WINAPI * EnumProcessModulesProc)(HANDLE, HMODULE *, DWORD, LPDWORD);
typedef DWORD(WINAPI * GetModuleBaseNameProc)(HANDLE, HMODULE, LPTSTR, DWORD);
typedef DWORD(WINAPI * GetModuleFileNameExProc)(HANDLE, HMODULE, LPTSTR, DWORD);

typedef BOOL(WINAPI * QueryFullProcessImageNameProc)(HANDLE hProcess, DWORD dwFlags, LPTSTR lpExeName, PDWORD lpdwSize);

BOOL GetProcessNameByPid1(DWORD dwProcessId, TCHAR szName[], DWORD nNameSize, TCHAR szPath[], DWORD nPathSize)
{
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { sizeof(pe) };
	BOOL fFound = FALSE;

	szPath[0] = '\0';
	szName[0] = '\0';

	if (Process32First(h, &pe))
	{
		do
		{
			if (pe.th32ProcessID == dwProcessId)
			{
				if (szName)
				{
					lstrcpyn(szName, pe.szExeFile, nNameSize);
				}

				if (szPath)
				{
					//OpenProcess(
					lstrcpyn(szPath, pe.szExeFile, nPathSize);
				}

				fFound = TRUE;
				break;
			}
		} while (Process32Next(h, &pe));
	}

	CloseHandle(h);

	return fFound;
}


//
// This uses PSAPI.DLL, which is only available under NT/2000/XP I think,
// so we dynamically load this library, so that we can still run under 9x.
//
//	dwProcessId  [in]
//  szName       [out]
//  nNameSize    [in]
//  szPath       [out]
//  nPathSize    [in]	
//
BOOL GetProcessNameByPid_BelowVista(DWORD dwProcessId, TCHAR szName[], DWORD nNameSize, TCHAR szPath[], DWORD nPathSize)
{
	HMODULE hPSAPI;
	HANDLE hProcess;

	HMODULE hModule;
	DWORD   dwNumModules;

	EnumProcessModulesProc  fnEnumProcessModules;
	GetModuleBaseNameProc   fnGetModuleBaseName;
	GetModuleFileNameExProc fnGetModuleFileNameEx;

	// Attempt to load Process Helper library
	hPSAPI = LoadLibrary(_T("psapi.dll"));

	if (!hPSAPI)
	{
		szName[0] = '\0';
		return FALSE;
	}

	// OK, we have access to the PSAPI functions, so open the process
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
	if (!hProcess)
	{
		FreeLibrary(hPSAPI);
		return FALSE;
	}


	fnEnumProcessModules = (EnumProcessModulesProc)GetProcAddress(hPSAPI, "EnumProcessModules");

#ifdef UNICODE	
	fnGetModuleBaseName = (GetModuleBaseNameProc)GetProcAddress(hPSAPI, "GetModuleBaseNameW");
	fnGetModuleFileNameEx = (GetModuleFileNameExProc)GetProcAddress(hPSAPI, "GetModuleFileNameExW");
#else
	fnGetModuleBaseName = (GetModuleBaseNameProc)GetProcAddress(hPSAPI, "GetModuleBaseNameA");
	fnGetModuleFileNameEx = (GetModuleFileNameExProc)GetProcAddress(hPSAPI, "GetModuleFileNameExA");
#endif

	if (!fnEnumProcessModules || !fnGetModuleBaseName)
	{
		CloseHandle(hProcess);
		FreeLibrary(hPSAPI);
		return FALSE;
	}

	// Find the first module
	if (fnEnumProcessModules(hProcess, &hModule, sizeof(hModule), &dwNumModules))
	{
		// Now get the module name
		if (szName)
			fnGetModuleBaseName(hProcess, hModule, szName, nNameSize);

		// get module filename
		if (szPath)
			fnGetModuleFileNameEx(hProcess, hModule, szPath, nPathSize);
	}
	else
	{
		CloseHandle(hProcess);
		FreeLibrary(hPSAPI);
		return FALSE;
	}

	CloseHandle(hProcess);
	FreeLibrary(hPSAPI);

	return TRUE;
}

BOOL GetProcessNameByPid(DWORD dwProcessId, TCHAR szName[], DWORD nNameSize, TCHAR szPath[], DWORD nPathSize)
{
	static QueryFullProcessImageNameProc fnQueryFullProcessImageName = NULL;
	HANDLE hProcess;
	DWORD dwSize;
	TCHAR *pName;
	BOOL bSucceeded;

	if (!fnQueryFullProcessImageName)
	{
#ifdef UNICODE	
		fnQueryFullProcessImageName = (QueryFullProcessImageNameProc)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "QueryFullProcessImageNameW");
#else
		fnQueryFullProcessImageName = (QueryFullProcessImageNameProc)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "QueryFullProcessImageNameA");
#endif

		if (!fnQueryFullProcessImageName)
			return GetProcessNameByPid_BelowVista(dwProcessId, szName, nNameSize, szPath, nPathSize);
	}

	bSucceeded = FALSE;

	hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
	if (hProcess)
	{
		dwSize = nPathSize;

		if (fnQueryFullProcessImageName(hProcess, 0, szPath, &dwSize))
		{
			pName = _tcsrchr(szPath, '\\');
			if (pName)
			{
				_tcsncpy_s(szName, nNameSize, pName + 1, _TRUNCATE);
				bSucceeded = TRUE;
			}
		}

		CloseHandle(hProcess);
	}

	return bSucceeded;
}

//
//	Update the Process tab for the specified window
//
void SetProcessInfo(HWND hwnd)
{
	DWORD dwProcessId;
	DWORD dwThreadId;
	TCHAR ach[32];
	TCHAR szPath[MAX_PATH];

	HWND  hwndDlg = WinSpyTab[PROCESS_TAB].hwnd;

	dwThreadId = GetWindowThreadProcessId(hwnd, &dwProcessId);

	// Process Id
	_stprintf_s(ach, ARRAYSIZE(ach), szHexFmt _T("  (%u)"), dwProcessId, dwProcessId);
	SetDlgItemText(hwndDlg, IDC_PID, ach);

	// Thread Id
	_stprintf_s(ach, ARRAYSIZE(ach), szHexFmt _T("  (%u)"), dwThreadId, dwThreadId);
	SetDlgItemText(hwndDlg, IDC_TID, ach);

	// Try to get process name and path
	if (GetProcessNameByPid(dwProcessId, ach, ARRAYSIZE(ach),
		szPath, ARRAYSIZE(szPath)))
	{
		SetDlgItemText(hwndDlg, IDC_PROCESSNAME, ach);
		SetDlgItemText(hwndDlg, IDC_PROCESSPATH, szPath);
	}
	else
	{
		SetDlgItemText(hwndDlg, IDC_PROCESSNAME, _T("N/A"));
		SetDlgItemText(hwndDlg, IDC_PROCESSPATH, _T("N/A"));
	}
}
