/*
  Copyright (C) 2011 Birunthan Mohanathas

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "StdAfx.h"
#include "DialogPackage.h"
#include "DialogInstall.h"
#include "resource.h"
#include "Application.h"

GlobalData g_Data;

OsNameVersion g_OsNameVersions[] =
{
	{ L"XP", L"5.1" },
	{ L"Vista", L"6.0" },
	{ L"7", L"6.1" },
//	{ L"8", L"6.2" }
};

/*
** Entry point
**
*/
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	// Avoid loading a dll from current directory
	SetDllDirectory(L"");

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	InitCommonControls();

	if (lpCmdLine[0] == L'"')
	{
		// Strip quotes
		++lpCmdLine;
		WCHAR* pos = wcsrchr(lpCmdLine, L'"');
		if (pos)
		{
			*pos = L'\0';
		}
	}

	WCHAR buffer[MAX_PATH];
	GetModuleFileName(hInstance, buffer, MAX_PATH);

	// Remove the module's name from the path
	WCHAR* pos = wcsrchr(buffer, L'\\');
	if (pos)
	{
		*(pos + 1) = L'\0';
	}

	g_Data.programPath = g_Data.settingsPath = buffer;
	wcscat(buffer, L"Rainmeter.ini");

	// Find the settings file and read skins path off it
	if (_waccess(buffer, 0) == 0)
	{
		g_Data.iniFile = buffer;
		if (GetPrivateProfileString(L"Rainmeter", L"SkinPath", L"", buffer, MAX_LINE_LENGTH, buffer) > 0)
		{
			g_Data.skinsPath = buffer;
			if (g_Data.skinsPath.back() != L'\\' && g_Data.skinsPath.back() != L'/')
			{
				g_Data.skinsPath += L'\\';
			}
		}
		else
		{
			g_Data.skinsPath = g_Data.programPath;
			g_Data.skinsPath += L"Skins\\";
		}
	}
	else
	{
		HRESULT hr = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buffer);
		wcscat(buffer, L"\\Rainmeter\\");
		g_Data.settingsPath = buffer;
		wcscat(buffer, L"Rainmeter.ini");
		g_Data.iniFile = buffer;
		if (SUCCEEDED(hr) && _waccess(buffer, 0) == 0)
		{
			if (GetPrivateProfileString(L"Rainmeter", L"SkinPath", L"", buffer, MAX_LINE_LENGTH, buffer) > 0)
			{
				g_Data.skinsPath = buffer;
				if (g_Data.skinsPath.back() != L'\\' && g_Data.skinsPath.back() != L'/')
				{
					g_Data.skinsPath += L'\\';
				}
			}
			else
			{
				std::wstring error = L"SkinPath not found.\nMake sure that Rainmeter has been run at least once.";
				MessageBox(NULL, error.c_str(), L"Rainmeter Skin Installer", MB_ERROR);
				return 1;
			}
		}
		else
		{
			std::wstring error = L"Rainmeter.ini not found.\nMake sure that Rainmeter has been run at least once.";
			MessageBox(NULL, error.c_str(), L"Rainmeter Skin Installer", MB_ERROR);
			return 1;
		}
	}

	if (_wcsnicmp(lpCmdLine, L"/LoadTheme ", 11) == 0)
	{
		// Skip "/LoadTheme "
		lpCmdLine += 11;

		if (*lpCmdLine && CloseRainmeterIfActive())
		{
			CDialogInstall::LoadTheme(lpCmdLine, true);

			std::wstring file = g_Data.programPath + L"Rainmeter.exe";
			SHELLEXECUTEINFO sei = {0};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = SEE_MASK_UNICODE;
			sei.lpFile = file.c_str();
			sei.lpDirectory = g_Data.programPath.c_str();
			sei.nShow = SW_SHOWNORMAL;
			ShellExecuteEx(&sei);
		}

		return 0;
	}
	else if (wcscmp(lpCmdLine, L"/Packager") == 0)
	{
		CDialogPackage::Create(hInstance, lpCmdLine);
	}
	else
	{
		CDialogInstall::Create(hInstance, lpCmdLine);
	}

	return 0;
}

bool CloseRainmeterIfActive()
{
	// Close Rainmeter.exe
	HWND hwnd = FindWindow(L"DummyRainWClass", L"Rainmeter control window");
	if (hwnd)
	{
		DWORD pID, exitCode;
		GetWindowThreadProcessId(hwnd, &pID);
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pID);
		PostMessage(hwnd, WM_DESTROY, 0, 0);

		// Wait up to 5 seconds for Rainmeter to close
		WaitForSingleObject(hProcess, 5000);
		GetExitCodeProcess(hProcess, &exitCode);
		CloseHandle(hProcess);

		if (exitCode == STILL_ACTIVE)
		{
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------------------------
// Stolen functions from Rainmeter Litestep.cpp, System.cpp, and Application.cpp
// -----------------------------------------------------------------------------------------------

bool IsRunning(const WCHAR* name, HANDLE* hMutex)
{
	// Create mutex
	HANDLE hMutexTmp = CreateMutex(NULL, FALSE, name);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		*hMutex = NULL;
		return true;
	}
	else
	{
		*hMutex = hMutexTmp;
		return false;
	}
}

bool CopyFiles(const std::wstring& strFrom, const std::wstring& strTo, bool bMove)
{
	std::wstring tmpFrom(strFrom), tmpTo(strTo);

	// The strings must end with double nul
	tmpFrom.append(1, L'\0');
	tmpTo.append(1, L'\0');

	SHFILEOPSTRUCT fo =
	{
		NULL,
		bMove ? FO_MOVE : FO_COPY,
		tmpFrom.c_str(),
		tmpTo.c_str(),
		FOF_NO_UI | FOF_NOCONFIRMATION | FOF_ALLOWUNDO
	};

	return SHFileOperation(&fo) == 0;
}

OSPLATFORM GetOSPlatform()
{
	OSVERSIONINFOEX osvi = {sizeof(OSVERSIONINFOEX)};
	if (GetVersionEx((OSVERSIONINFO*)&osvi))
	{
		if (osvi.dwMajorVersion == 5)
		{
			// Not checking for osvi.dwMinorVersion >= 1 because Rainmeter won't run on pre-XP
			return OSPLATFORM_XP;
		}
		else if (osvi.dwMajorVersion == 6)
		{
			if (osvi.dwMinorVersion == 0)
			{
				return OSPLATFORM_VISTA; // Vista, Server 2008
			}
			else
			{
				return OSPLATFORM_7; // 7, Server 2008R2
			}
		}
		else // newer OS
		{
			return OSPLATFORM_7;
		}
	}

	return OSPLATFORM_UNKNOWN;
}

std::string ConvertToAscii(LPCTSTR str)
{
	std::string szAscii;

	if (str && *str)
	{
		int strLen = (int)wcslen(str);
		int bufLen = WideCharToMultiByte(CP_ACP, 0, str, strLen, NULL, 0, NULL, NULL);
		if (bufLen > 0)
		{
			szAscii.resize(bufLen);
			WideCharToMultiByte(CP_ACP, 0, str, strLen, &szAscii[0], bufLen, NULL, NULL);
		}
	}
	return szAscii;
}

std::wstring ConvertToWide(LPCSTR str)
{
	std::wstring szWide;

	if (str && *str)
	{
		int strLen = (int)strlen(str);
		int bufLen = MultiByteToWideChar(CP_ACP, 0, str, strLen, NULL, 0);
		if (bufLen > 0)
		{
			szWide.resize(bufLen);
			MultiByteToWideChar(CP_ACP, 0, str, strLen, &szWide[0], bufLen);
		}
	}
	return szWide;
}