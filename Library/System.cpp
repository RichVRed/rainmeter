/*
  Copyright (C) 2010 spx

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
#include "System.h"
#include "Litestep.h"
#include "Rainmeter.h"
#include "MeterWindow.h"
#include "MeasureNet.h"
#include "Error.h"

using namespace Gdiplus;

#define DEBUG_VERBOSE  (0)  // Set 1 if you need verbose logging.

#define ZPOS_FLAGS	(SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING)

enum TIMER
{
	TIMER_SHOWDESKTOP   = 1,
	TIMER_RESUME        = 2
};
enum INTERVAL
{
	INTERVAL_SHOWDESKTOP    = 250,
	INTERVAL_RESTOREWINDOWS = 100,
	INTERVAL_RESUME         = 1000
};

MultiMonitorInfo CSystem::c_Monitors = { 0 };

HWND CSystem::c_Window = NULL;
HWND CSystem::c_HelperWindow = NULL;

HWINEVENTHOOK CSystem::c_WinEventHook = NULL;

bool CSystem::c_ShowDesktop = false;

OSPLATFORM CSystem::c_Platform = OSPLATFORM_UNKNOWN;

std::wstring CSystem::c_WorkingDirectory;

std::vector<std::wstring> CSystem::c_IniFileMappings;

extern CRainmeter* Rainmeter;

/*
** Creates a helper window to detect changes in the system.
**
*/
void CSystem::Initialize(HINSTANCE instance)
{
	WNDCLASS wc = {0};
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.hInstance = instance;
	wc.lpszClassName = L"RainmeterSystem";
	ATOM className = RegisterClass(&wc);

	c_Window = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		MAKEINTATOM(className),
		L"System",
		WS_POPUP | WS_DISABLED,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		instance,
		NULL);

	c_HelperWindow = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		MAKEINTATOM(className),
		L"PositioningHelper",
		WS_POPUP | WS_DISABLED,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		instance,
		NULL);

	SetWindowPos(c_Window, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);
	SetWindowPos(c_HelperWindow, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);

	SetOSPlatform();

	c_Monitors.monitors.reserve(4);
	SetMultiMonitorInfo();

	WCHAR directory[MAX_PATH];
	DWORD len = GetCurrentDirectory(MAX_PATH, directory);
	c_WorkingDirectory.assign(directory, len <= MAX_PATH ? len : 0);

	c_WinEventHook = SetWinEventHook(
		EVENT_SYSTEM_FOREGROUND,
		EVENT_SYSTEM_FOREGROUND,
		NULL,
		MyWinEventProc,
		0,
		0,
		WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

	SetTimer(c_Window, TIMER_SHOWDESKTOP, INTERVAL_SHOWDESKTOP, NULL);
}

/*
** Destroys a window.
**
*/
void CSystem::Finalize()
{
	KillTimer(c_Window, TIMER_SHOWDESKTOP);
	KillTimer(c_Window, TIMER_RESUME);

	if (c_WinEventHook)
	{
		UnhookWinEvent(c_WinEventHook);
		c_WinEventHook = NULL;
	}

	if (c_HelperWindow)
	{
		DestroyWindow(c_HelperWindow);
		c_HelperWindow = NULL;
	}

	if (c_Window)
	{
		DestroyWindow(c_Window);
		c_Window = NULL;
	}
}

/*
** Retrieves the multi-monitor information.
**
*/
BOOL CALLBACK MyInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	MultiMonitorInfo* m = (MultiMonitorInfo*)dwData;

	MONITORINFOEX info;
	info.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &info);

	if (Rainmeter->GetDebug())
	{
		Log(LOG_DEBUG, info.szDevice);
		LogWithArgs(LOG_DEBUG, L"  Flags    : %s(0x%08X)", (info.dwFlags & MONITORINFOF_PRIMARY) ? L"PRIMARY " : L"", info.dwFlags);
		LogWithArgs(LOG_DEBUG, L"  Handle   : 0x%p", hMonitor);
		LogWithArgs(LOG_DEBUG, L"  ScrArea  : L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
			lprcMonitor->left, lprcMonitor->top, lprcMonitor->right, lprcMonitor->bottom,
			lprcMonitor->right - lprcMonitor->left, lprcMonitor->bottom - lprcMonitor->top);
		LogWithArgs(LOG_DEBUG, L"  WorkArea : L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
			info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom,
			info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top);
	}
	if (m == NULL) return TRUE;

	if (m->useEnumDisplayDevices)
	{
		for (size_t i = 0, isize = m->monitors.size(); i < isize; ++i)
		{
			if (m->monitors[i].handle == NULL && _wcsicmp(info.szDevice, m->monitors[i].deviceName.c_str()) == 0)
			{
				m->monitors[i].handle = hMonitor;
				m->monitors[i].screen = *lprcMonitor;
				m->monitors[i].work = info.rcWork;
				break;
			}
		}
	}
	else  // use only EnumDisplayMonitors
	{
		MonitorInfo monitor;
		monitor.active = true;

		monitor.handle = hMonitor;
		monitor.screen = *lprcMonitor;
		monitor.work = info.rcWork;

		monitor.deviceName = info.szDevice;  // E.g. "\\.\DISPLAY1"

		// Get the monitor name (E.g. "Generic Non-PnP Monitor")
		DISPLAY_DEVICE ddm = {sizeof(DISPLAY_DEVICE)};
		DWORD dwMon = 0;
		while (EnumDisplayDevices(info.szDevice, dwMon++, &ddm, 0))
		{
			if (ddm.StateFlags & DISPLAY_DEVICE_ACTIVE && ddm.StateFlags & DISPLAY_DEVICE_ATTACHED)
			{
				monitor.monitorName.assign(ddm.DeviceString, wcsnlen(ddm.DeviceString, _countof(ddm.DeviceString)));
				break;
			}
		}

		m->monitors.push_back(monitor);

		if (info.dwFlags & MONITORINFOF_PRIMARY)
		{
			// It's primary monitor!
			m->primary = (int)m->monitors.size();
		}
	}

	return TRUE;
}

/*
** Returns the number of monitors.
**
*/
size_t CSystem::GetMonitorCount()
{
	if (c_Monitors.monitors.empty())
	{
		SetMultiMonitorInfo();
	}
	return c_Monitors.monitors.size();
}

/*
** Sets the multi-monitor information.
**
*/
void CSystem::SetMultiMonitorInfo()
{
	std::vector<MonitorInfo>& monitors = c_Monitors.monitors;
	bool logging = Rainmeter->GetDebug();

	c_Monitors.vsT = GetSystemMetrics(SM_YVIRTUALSCREEN);
	c_Monitors.vsL = GetSystemMetrics(SM_XVIRTUALSCREEN);
	c_Monitors.vsH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	c_Monitors.vsW = GetSystemMetrics(SM_CXVIRTUALSCREEN);

	c_Monitors.primary = 1;  // If primary screen is not found, 1st screen is assumed as primary screen.

	c_Monitors.useEnumDisplayDevices = true;
	c_Monitors.useEnumDisplayMonitors = false;

	if (logging)
	{
		Log(LOG_DEBUG, L"------------------------------");
		Log(LOG_DEBUG, L"* EnumDisplayDevices / EnumDisplaySettings API");
	}

	DISPLAY_DEVICE dd = {sizeof(DISPLAY_DEVICE)};

	if (EnumDisplayDevices(NULL, 0, &dd, 0))
	{
		DWORD dwDevice = 0;

		do
		{
			std::wstring msg;

			std::wstring deviceName(dd.DeviceName, wcsnlen(dd.DeviceName, _countof(dd.DeviceName)));
			std::wstring deviceString;

			if (logging)
			{
				deviceString.assign(dd.DeviceString, wcsnlen(dd.DeviceString, _countof(dd.DeviceString)));

				Log(LOG_DEBUG, deviceName.c_str());

				if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE)
				{
					msg += L"ACTIVE ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_MULTI_DRIVER)
				{
					msg += L"MULTI ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
				{
					msg += L"PRIMARY ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
				{
					msg += L"MIRROR ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE)
				{
					msg += L"VGA ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_REMOVABLE)
				{
					msg += L"REMOVABLE ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_MODESPRUNED)
				{
					msg += L"PRUNED ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_REMOTE)
				{
					msg += L"REMOTE ";
				}
				if (dd.StateFlags & DISPLAY_DEVICE_DISCONNECT)
				{
					msg += L"DISCONNECT ";
				}
			}

			if ((dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) == 0)
			{
				MonitorInfo monitor = {0};

				monitor.handle = NULL;
				monitor.deviceName = deviceName;  // E.g. "\\.\DISPLAY1"

				// Get the monitor name (E.g. "Generic Non-PnP Monitor")
				DISPLAY_DEVICE ddm = {sizeof(DISPLAY_DEVICE)};
				DWORD dwMon = 0;
				while (EnumDisplayDevices(deviceName.c_str(), dwMon++, &ddm, 0))
				{
					if (ddm.StateFlags & DISPLAY_DEVICE_ACTIVE && ddm.StateFlags & DISPLAY_DEVICE_ATTACHED)
					{
						monitor.monitorName.assign(ddm.DeviceString, wcsnlen(ddm.DeviceString, _countof(ddm.DeviceString)));

						if (logging)
						{
							LogWithArgs(LOG_DEBUG, L"  Name     : %s", monitor.monitorName.c_str());
						}
						break;
					}
				}

				if (logging)
				{
					LogWithArgs(LOG_DEBUG, L"  Adapter  : %s", deviceString.c_str());
					LogWithArgs(LOG_DEBUG, L"  Flags    : %s(0x%08X)", msg.c_str(), dd.StateFlags);
				}

				if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE)
				{
					monitor.active = true;

					DEVMODE dm = {0};
					dm.dmSize = sizeof(DEVMODE);

					if (EnumDisplaySettings(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &dm))
					{
						POINT pos = {dm.dmPosition.x, dm.dmPosition.y};
						monitor.handle = MonitorFromPoint(pos, MONITOR_DEFAULTTONULL);

						if (logging)
						{
							LogWithArgs(LOG_DEBUG, L"  Handle   : 0x%p", monitor.handle);
						}
					}

					if (monitor.handle != NULL)
					{
						MONITORINFO info = {sizeof(MONITORINFO)};
						GetMonitorInfo(monitor.handle, &info);

						monitor.screen = info.rcMonitor;
						monitor.work = info.rcWork;

						if (logging)
						{
							LogWithArgs(LOG_DEBUG, L"  ScrArea  : L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
								info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom,
								info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top);
							LogWithArgs(LOG_DEBUG, L"  WorkArea : L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
								info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom,
								info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top);
						}
					}
					else  // monitor not found
					{
						c_Monitors.useEnumDisplayMonitors = true;
					}
				}
				else
				{
					monitor.active = false;
				}

				monitors.push_back(monitor);

				if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
				{
					// It's primary monitor!
					c_Monitors.primary = (int)monitors.size();
				}
			}
			else
			{
				if (logging)
				{
					LogWithArgs(LOG_DEBUG, L"  Adapter  : %s", deviceString.c_str());
					LogWithArgs(LOG_DEBUG, L"  Flags    : %s(0x%08X)", msg.c_str(), dd.StateFlags);
				}
			}
			++dwDevice;
		}
		while (EnumDisplayDevices(NULL, dwDevice, &dd, 0));
	}

	if (monitors.empty())  // Failed to enumerate the non-mirroring monitors
	{
		Log(LOG_WARNING, L"Failed to enumerate the non-mirroring monitors. Only EnumDisplayMonitors is used instead.");
		c_Monitors.useEnumDisplayDevices = false;
		c_Monitors.useEnumDisplayMonitors = true;
	}

	if (logging)
	{
		Log(LOG_DEBUG, L"------------------------------");
		Log(LOG_DEBUG, L"* EnumDisplayMonitors API");
	}

	if (c_Monitors.useEnumDisplayMonitors)
	{
		EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, (LPARAM)(&c_Monitors));

		if (monitors.empty())  // Failed to enumerate the monitors
		{
			Log(LOG_WARNING, L"Failed to enumerate monitors. Using dummy monitor info.");
			c_Monitors.useEnumDisplayMonitors = false;

			MonitorInfo monitor;
			monitor.active = true;

			POINT pos = {0, 0};
			monitor.handle = MonitorFromPoint(pos, MONITOR_DEFAULTTOPRIMARY);
			monitor.screen.left = 0;
			monitor.screen.top = 0;
			monitor.screen.right = GetSystemMetrics(SM_CXSCREEN);
			monitor.screen.bottom = GetSystemMetrics(SM_CYSCREEN);
			if (SystemParametersInfo(SPI_GETWORKAREA, 0, &(monitor.work), 0) == 0)  // failed
			{
				monitor.work = monitor.screen;
			}

			monitor.deviceName = L"DUMMY";

			monitors.push_back(monitor);

			c_Monitors.primary = 1;
		}
	}
	else
	{
		if (logging)
		{
			EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, (LPARAM)NULL);  // Only logging
		}
	}

	if (logging)
	{
		Log(LOG_DEBUG, L"------------------------------");

		std::wstring method = L"* METHOD: ";
		if (c_Monitors.useEnumDisplayDevices)
		{
			method += L"EnumDisplayDevices + ";
			method += c_Monitors.useEnumDisplayMonitors ? L"EnumDisplayMonitors Mode" : L"EnumDisplaySettings Mode";
		}
		else
		{
			method += c_Monitors.useEnumDisplayMonitors ? L"EnumDisplayMonitors Mode" : L"Dummy Mode";
		}
		Log(LOG_DEBUG, method.c_str());

		LogWithArgs(LOG_DEBUG, L"* MONITORS: Count=%i, Primary=@%i", (int)monitors.size(), c_Monitors.primary);
		Log(LOG_DEBUG, L"@0: Virtual screen");
		LogWithArgs(LOG_DEBUG, L"  L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
			c_Monitors.vsL, c_Monitors.vsT, c_Monitors.vsL + c_Monitors.vsW, c_Monitors.vsT + c_Monitors.vsH,
			c_Monitors.vsW, c_Monitors.vsH);

		for (size_t i = 0, isize = monitors.size(); i < isize; ++i)
		{
			if (monitors[i].active)
			{
				LogWithArgs(LOG_DEBUG, L"@%i: %s (active), MonitorName: %s", (int)i + 1, monitors[i].deviceName.c_str(), monitors[i].monitorName.c_str());
				LogWithArgs(LOG_DEBUG, L"  L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
					monitors[i].screen.left, monitors[i].screen.top, monitors[i].screen.right, monitors[i].screen.bottom,
					monitors[i].screen.right - monitors[i].screen.left, monitors[i].screen.bottom - monitors[i].screen.top);
			}
			else
			{
				LogWithArgs(LOG_DEBUG, L"@%i: %s (inactive), MonitorName: %s", (int)i + 1, monitors[i].deviceName.c_str(), monitors[i].monitorName.c_str());
			}
		}
		Log(LOG_DEBUG, L"------------------------------");
	}
}

/*
** Updates the workarea information.
**
*/
void CSystem::UpdateWorkareaInfo()
{
	std::vector<MonitorInfo>& monitors = c_Monitors.monitors;

	if (monitors.empty())
	{
		SetMultiMonitorInfo();
		return;
	}

	for (size_t i = 0, isize = monitors.size(); i < isize; ++i)
	{
		if (monitors[i].active && monitors[i].handle != NULL)
		{
			MONITORINFO info = {sizeof(MONITORINFO)};
			GetMonitorInfo(monitors[i].handle, &info);

			monitors[i].work = info.rcWork;

			if (Rainmeter->GetDebug())
			{
				LogWithArgs(LOG_DEBUG, L"WorkArea@%i : L=%i, T=%i, R=%i, B=%i (W=%i, H=%i)",
					(int)i + 1,
					info.rcWork.left, info.rcWork.top, info.rcWork.right, info.rcWork.bottom,
					info.rcWork.right - info.rcWork.left, info.rcWork.bottom - info.rcWork.top);
			}
		}
	}
}

/*
** Sets the OS platform.
**
*/
void CSystem::SetOSPlatform()
{
	OSVERSIONINFOEX osvi = {sizeof(OSVERSIONINFOEX)};
	if (GetVersionEx((OSVERSIONINFO*)&osvi))
	{
		if (osvi.dwMajorVersion == 5)
		{
			// Not checking for osvi.dwMinorVersion >= 1 because Rainmeter won't run on pre-XP
			c_Platform = OSPLATFORM_XP;
		}
		else if (osvi.dwMajorVersion == 6)
		{
			if (osvi.dwMinorVersion == 0)
			{
				c_Platform = OSPLATFORM_VISTA; // Vista, Server 2008
			}
			else
			{
				c_Platform = OSPLATFORM_7; // 7, Server 2008R2
			}
		}
		else // newer OS
		{
			c_Platform = OSPLATFORM_7;
		}
	}
}

/*
** Finds the Default Shell's window.
**
*/
HWND CSystem::GetDefaultShellWindow()
{
	static HWND c_ShellW = NULL;  // cache
	HWND ShellW = GetShellWindow();

	if (ShellW)
	{
		if (ShellW == c_ShellW)
		{
			return ShellW;
		}
		else
		{
			const int classLen = _countof(L"Progman");
			WCHAR className[classLen];
			if (!(GetClassName(ShellW, className, classLen) > 0 &&
				wcscmp(className, L"Progman") == 0))
			{
				ShellW = NULL;
			}
		}
	}

	c_ShellW = ShellW;
	return ShellW;
}

/*
** Finds the WorkerW window.
** If the WorkerW window is not active, returns NULL.
**
*/
HWND CSystem::GetWorkerW()
{
	static HWND c_DefView = NULL;  // cache
	HWND ShellW = GetDefaultShellWindow();
	if (!ShellW) return NULL;  // Default Shell (Explorer) not running

	if (c_DefView && IsWindow(c_DefView))
	{
		HWND parent = GetAncestor(c_DefView, GA_PARENT);
		if (parent)
		{
			if (parent == ShellW)
			{
				return NULL;
			}
			else
			{
				const int classLen = _countof(L"WorkerW");
				WCHAR className[classLen];
				if (GetClassName(parent, className, classLen) > 0 &&
					wcscmp(className, L"WorkerW") == 0)
				{
					return parent;
				}
			}
		}
	}

	HWND WorkerW = NULL, DefView = FindWindowEx(ShellW, NULL, L"SHELLDLL_DefView", L"");
	if (DefView == NULL)
	{
		while (WorkerW = FindWindowEx(NULL, WorkerW, L"WorkerW", L""))
		{
			if (IsWindowVisible(WorkerW) &&
				BelongToSameProcess(ShellW, WorkerW) &&
				(DefView = FindWindowEx(WorkerW, NULL, L"SHELLDLL_DefView", L"")))
			{
				break;
			}
		}
	}

	c_DefView = DefView;
	return WorkerW;
}

/*
** Returns the first window whose position is not ZPOSITION_ONDESKTOP,
** ZPOSITION_BOTTOM, or ZPOSITION_NORMAL.
**
*/
HWND CSystem::GetBackmostTopWindow()
{
	HWND winPos = c_HelperWindow;

	// Skip all ZPOSITION_ONDESKTOP, ZPOSITION_BOTTOM, and ZPOSITION_NORMAL windows
	while (winPos = ::GetNextWindow(winPos, GW_HWNDPREV))
	{
		CMeterWindow* wnd = Rainmeter->GetMeterWindow(winPos);
		if (!wnd ||
			(wnd->GetWindowZPosition() != ZPOSITION_NORMAL && 
			wnd->GetWindowZPosition() != ZPOSITION_ONDESKTOP &&
			wnd->GetWindowZPosition() != ZPOSITION_ONBOTTOM))
		{
			break;
		}
	}

	return winPos;
}

/*
** Checks whether the given windows belong to the same process.
**
*/
bool CSystem::BelongToSameProcess(HWND hwndA, HWND hwndB)
{
	DWORD procAId = 0, procBId = 0;

	GetWindowThreadProcessId(hwndA, &procAId);
	GetWindowThreadProcessId(hwndB, &procBId);

	return (procAId == procBId);
}

/*
** Retrieves the Rainmeter's meter windows in Z-order.
**
*/
BOOL CALLBACK MyEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	bool logging = Rainmeter->GetDebug() && DEBUG_VERBOSE;
	const int classLen = _countof(METERWINDOW_CLASS_NAME) + (DEBUG_VERBOSE ? 32 : 0);
	WCHAR className[classLen];
	CMeterWindow* Window;
	WCHAR flag;

	if (GetClassName(hwnd, className, classLen) > 0 &&
		wcscmp(className, METERWINDOW_CLASS_NAME) == 0 &&
		(Window = Rainmeter->GetMeterWindow(hwnd)))
	{
		ZPOSITION zPos = Window->GetWindowZPosition();
		if (zPos == ZPOSITION_ONDESKTOP ||
			(zPos == ZPOSITION_NORMAL && Rainmeter->IsNormalStayDesktop()) ||
			zPos == ZPOSITION_ONBOTTOM)
		{
			if (lParam)
			{
				((std::vector<CMeterWindow*>*)lParam)->push_back(Window);
			}

			if (logging) flag = L'+';
		}
		else
		{
			if (logging) flag = L'-';
		}

		if (logging)
		{
			LogWithArgs(LOG_DEBUG, L"%c [%c] 0x%p : %s (Name: \"%s\", zPos=%i)",
				flag, IsWindowVisible(hwnd) ? L'V' : L'H', hwnd, className, Window->GetFolderPath().c_str(), (int)zPos);
		}
	}
	else
	{
		if (logging)
		{
			flag = (hwnd == CSystem::GetHelperWindow()) ? L'o' : ' ';
			LogWithArgs(LOG_DEBUG, L"%c [%c] 0x%p : %s", flag, IsWindowVisible(hwnd) ? L'V' : L'H', hwnd, className);
		}
	}

	return TRUE;
}

/*
** Arranges the meter window in Z-order.
**
*/
void CSystem::ChangeZPosInOrder()
{
	bool logging = Rainmeter->GetDebug() && DEBUG_VERBOSE;
	std::vector<CMeterWindow*> windowsInZOrder;

	if (logging) Log(LOG_DEBUG, L"1: ----- BEFORE -----");

	// Retrieve the Rainmeter's meter windows in Z-order
	EnumWindows(MyEnumWindowsProc, (LPARAM)(&windowsInZOrder));

	auto resetZPos = [&](ZPOSITION zpos)
	{
		// Reset ZPos in Z-order (Bottom)
		std::vector<CMeterWindow*>::const_iterator iter = windowsInZOrder.begin();
		for ( ; iter != windowsInZOrder.end(); ++iter)
		{
			if ((*iter)->GetWindowZPosition() == zpos)
			{
				(*iter)->ChangeZPos(zpos);  // reset
			}
		}
	};

	if (Rainmeter->IsNormalStayDesktop())
	{
		resetZPos(ZPOSITION_NORMAL);
	}

	if (!c_ShowDesktop)
	{
		resetZPos(ZPOSITION_ONBOTTOM);
	}

	resetZPos(ZPOSITION_ONDESKTOP);

	if (logging)
	{
		Log(LOG_DEBUG, L"2: ----- AFTER -----");

		// Log all windows in Z-order
		EnumWindows(MyEnumWindowsProc, (LPARAM)NULL);
	}
}

/*
** Moves the helper window to the reference position.
**
*/
void CSystem::PrepareHelperWindow(HWND WorkerW)
{
	bool logging = Rainmeter->GetDebug() && DEBUG_VERBOSE;

	SetWindowPos(c_Window, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);  // always on bottom

	if (c_ShowDesktop && WorkerW)
	{
		// Set WS_EX_TOPMOST flag
		SetWindowPos(c_HelperWindow, HWND_TOPMOST, 0, 0, 0, 0, ZPOS_FLAGS);

		// Find the "backmost" topmost window
		HWND hwnd = WorkerW;
		while (hwnd = ::GetNextWindow(hwnd, GW_HWNDPREV))
		{
			if (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
			{
				WCHAR className[64], windowText[64];

				if (logging)
				{
					GetClassName(hwnd, className, 64);
					GetWindowText(hwnd, windowText, 64);

					SetLastError(ERROR_SUCCESS);
				}

				// Insert the helper window after the found window
				if (0 != SetWindowPos(c_HelperWindow, hwnd, 0, 0, 0, 0, ZPOS_FLAGS))
				{
					if (logging)
					{
						LogWithArgs(LOG_DEBUG, L"System: HelperWindow: hwnd=0x%p (WorkerW=0x%p), hwndInsertAfter=0x%p (\"%s\" %s) - %s",
							c_HelperWindow, WorkerW, hwnd, windowText, className, (GetWindowLongPtr(c_HelperWindow, GWL_EXSTYLE) & WS_EX_TOPMOST) ? L"TOPMOST" : L"NORMAL");
					}
					return;
				}

				if (logging)
				{
					DWORD err = GetLastError();
					LogWithArgs(LOG_DEBUG, L"System: HelperWindow: hwnd=0x%p (WorkerW=0x%p), hwndInsertAfter=0x%p (\"%s\" %s) - FAILED (ErrorCode=0x%08X)",
						c_HelperWindow, WorkerW, hwnd, windowText, className, err);
				}
			}
		}

		if (logging)
		{
			LogWithArgs(LOG_DEBUG, L"System: HelperWindow: hwnd=0x%p (WorkerW=0x%p), hwndInsertAfter=HWND_TOPMOST - %s",
				c_HelperWindow, WorkerW, (GetWindowLongPtr(c_HelperWindow, GWL_EXSTYLE) & WS_EX_TOPMOST) ? L"TOPMOST" : L"NORMAL");
		}
	}
	else
	{
		// Insert the helper window to the bottom
		SetWindowPos(c_HelperWindow, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);

		if (logging)
		{
			LogWithArgs(LOG_DEBUG, L"System: HelperWindow: hwnd=0x%p (WorkerW=0x%p), hwndInsertAfter=HWND_BOTTOM - %s",
				c_HelperWindow, WorkerW, (GetWindowLongPtr(c_HelperWindow, GWL_EXSTYLE) & WS_EX_TOPMOST) ? L"TOPMOST" : L"NORMAL");
		}
	}
}

/*
** Changes the "Show Desktop" state.
**
*/
bool CSystem::CheckDesktopState(HWND WorkerW)
{
	HWND hwnd = NULL;

	if (WorkerW && IsWindowVisible(WorkerW))
	{
		hwnd = FindWindowEx(NULL, WorkerW, L"RainmeterSystem", L"System");
	}

	bool stateChanged = (hwnd && !c_ShowDesktop) || (!hwnd && c_ShowDesktop);

	if (stateChanged)
	{
		c_ShowDesktop = !c_ShowDesktop;

		if (Rainmeter->GetDebug())
		{
			LogWithArgs(LOG_DEBUG, L"System: \"Show %s\" has been detected.",
				c_ShowDesktop ? L"desktop" : L"open windows");
		}

		PrepareHelperWindow(WorkerW);

		ChangeZPosInOrder();

		if (c_ShowDesktop)
		{
			SetTimer(c_Window, TIMER_SHOWDESKTOP, INTERVAL_RESTOREWINDOWS, NULL);
		}
		else
		{
			SetTimer(c_Window, TIMER_SHOWDESKTOP, INTERVAL_SHOWDESKTOP, NULL);
		}
	}

	return stateChanged;
}

/*
** The event hook procedure
**
*/
void CALLBACK CSystem::MyWinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
	if (event == EVENT_SYSTEM_FOREGROUND)
	{
		if (!c_ShowDesktop)
		{
			const int classLen = _countof(L"WorkerW");
			WCHAR className[classLen];
			if (GetClassName(hwnd, className, classLen) > 0 &&
				wcscmp(className, L"WorkerW") == 0 &&
				BelongToSameProcess(GetDefaultShellWindow(), hwnd))
			{
				const int max = 5;
				int loop = 0;
				while (loop < max && FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", L"") == NULL)
				{
					Sleep(2);  // Wait for 2-16 ms before retrying
					++loop;
				}

				if (loop < max)
				{
					loop = 0;
					while (loop < max && !CheckDesktopState(hwnd))
					{
						Sleep(2);  // Wait for 2-16 ms before retrying
						++loop;
					}
				}
			}
		}
	}
}

/*
** The window procedure
**
*/
LRESULT CALLBACK CSystem::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (hWnd != c_Window)
	{
		if (uMsg == WM_WINDOWPOSCHANGING)
		{
			((LPWINDOWPOS)lParam)->flags |= SWP_NOZORDER;
			return 0;
		}
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_WINDOWPOSCHANGING:
		((LPWINDOWPOS)lParam)->flags |= SWP_NOZORDER;
		break;

	case WM_TIMER:
		switch (wParam)
		{
		case TIMER_SHOWDESKTOP:
			if (wParam == TIMER_SHOWDESKTOP)
			{
				CheckDesktopState(GetWorkerW());
			}
			break;

		case TIMER_RESUME:
			KillTimer(hWnd, TIMER_RESUME);
			if (Rainmeter->IsRedrawable())
			{
				std::map<std::wstring, CMeterWindow*>::const_iterator iter = Rainmeter->GetAllMeterWindows().begin();
				for ( ; iter != Rainmeter->GetAllMeterWindows().end(); ++iter)
				{
					(*iter).second->RedrawWindow();
				}
			}
			break;
		}
		break;

	case WM_DISPLAYCHANGE:
		Log(LOG_NOTICE, L"System: Display settings changed");
		ClearMultiMonitorInfo();
		CConfigParser::ClearMultiMonitorVariables();
	case WM_SETTINGCHANGE:
		if (uMsg == WM_DISPLAYCHANGE || (uMsg == WM_SETTINGCHANGE && wParam == SPI_SETWORKAREA))
		{
			if (uMsg == WM_SETTINGCHANGE)  // SPI_SETWORKAREA
			{
				Log(LOG_NOTICE, L"System: Work area changed");
				UpdateWorkareaInfo();
				CConfigParser::UpdateWorkareaVariables();
			}

			// Deliver WM_DISPLAYCHANGE / WM_SETTINGCHANGE message to all meter windows
			std::map<std::wstring, CMeterWindow*>::const_iterator iter = Rainmeter->GetAllMeterWindows().begin();
			for ( ; iter != Rainmeter->GetAllMeterWindows().end(); ++iter)
			{
				PostMessage((*iter).second->GetWindow(), WM_METERWINDOW_DELAYED_MOVE, (WPARAM)uMsg, (LPARAM)0);
			}
		}
		break;

	case WM_POWERBROADCAST:
		if (wParam == PBT_APMRESUMESUSPEND)
		{
			// Deliver PBT_APMRESUMESUSPEND event to all meter windows
			SetTimer(hWnd, TIMER_RESUME, INTERVAL_RESUME, NULL);
		}
		return TRUE;

	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

/*
** Retrieves the number of milliseconds that have elapsed since the system was started.
** In XP, returns the predictive value due to the 32bit limitation.
**
*/
ULONGLONG CSystem::GetTickCount64()
{
	typedef ULONGLONG (WINAPI * FPGETTICKCOUNT64)();
	static FPGETTICKCOUNT64 c_GetTickCount64 = (FPGETTICKCOUNT64)GetProcAddress(GetModuleHandle(L"kernel32"), "GetTickCount64");

	if (c_GetTickCount64)
	{
		return c_GetTickCount64();
	}
	else
	{
		static ULONGLONG lastTicks = 0;
		ULONGLONG ticks = GetTickCount();
		while (ticks < lastTicks) ticks += 0x100000000;
		lastTicks = ticks;
		return ticks;
	}
}

/*
** Checks if file is writable.
**
*/
bool CSystem::IsFileWritable(LPCWSTR file)
{
	HANDLE hFile = CreateFile(file, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	CloseHandle(hFile);
	return true;
}

/*
** This function is a wrapper function for LoadLibrary().
**
** Avoids loading a DLL from current directory.
**
*/
HMODULE CSystem::RmLoadLibrary(LPCWSTR lpLibFileName, DWORD* dwError, bool ignoreErrors)
{
	UINT oldMode;

	if (ignoreErrors)
	{
		oldMode = SetErrorMode(0);
		SetErrorMode(oldMode | SEM_FAILCRITICALERRORS);  // Prevent the system from displaying message box
	}

	// Remove current directory from DLL search path
	SetDllDirectory(L"");

	SetLastError(ERROR_SUCCESS);
	HMODULE hLib = LoadLibrary(lpLibFileName);

	if (dwError)
	{
		*dwError = GetLastError();
	}

	if (ignoreErrors)
	{
		SetErrorMode(oldMode);  // Reset
	}

	return hLib;
}

/*
** Resets working directory to default.
**
*/
void CSystem::ResetWorkingDirectory()
{
	WCHAR directory[MAX_PATH] = {0};
	GetCurrentDirectory(MAX_PATH, directory);

	const WCHAR* workDir = c_WorkingDirectory.c_str();
	if (_wcsicmp(directory, workDir) != 0)
	{
		SetCurrentDirectory(workDir);
	}
}

/*
** Sets clipboard text to given string.
**
*/
void CSystem::SetClipboardText(const std::wstring& text)
{
	if (OpenClipboard(NULL))
	{
		// Include terminating null char
		size_t len = text.length() + 1;

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
		if (hMem)
		{
			LPVOID data = GlobalLock(hMem);
			memcpy(data, text.c_str(), len * sizeof(WCHAR));
			GlobalUnlock(hMem);

			EmptyClipboard();
			if (!SetClipboardData(CF_UNICODETEXT, hMem))
			{
				GlobalFree(hMem);
			}
		}

		CloseClipboard();
	}
}

/*
** Sets the system wallpapar.
**
*/
void CSystem::SetWallpaper(const std::wstring& wallpaper, const std::wstring& style)
{
	if (!wallpaper.empty())
	{
		if (_waccess(wallpaper.c_str(), 0) == -1)
		{
			LogWithArgs(LOG_ERROR, L"!SetWallpaper: Unable to read file: %s", wallpaper.c_str());
			return;
		}

		Bitmap* bitmap = Bitmap::FromFile(wallpaper.c_str());
		if (bitmap && bitmap->GetLastStatus() == Ok)
		{
			std::wstring file = Rainmeter->GetSettingsPath() + L"Wallpaper.bmp";

			const CLSID bmpClsid = { 0x557cf400, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x0, 0x0, 0xf8, 0x1e, 0xf3, 0x2e } };
			if (bitmap->Save(file.c_str(), &bmpClsid) == Ok)
			{
				if (!style.empty())
				{
					HKEY hKey;
					if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
					{
						const WCHAR* wallStyle = NULL;
						const WCHAR* wallTile = L"0";

						const WCHAR* option = style.c_str();
						if (_wcsicmp(option, L"CENTER") == 0)
						{
							wallStyle = L"0";
						}
						else if (_wcsicmp(option, L"TILE") == 0)
						{
							wallStyle = L"0";
							wallTile = L"1";
						}
						else if (_wcsicmp(option, L"STRETCH") == 0)
						{
							wallStyle = L"2";
						}
						else if (GetOSPlatform() >= OSPLATFORM_7)
						{
							if (_wcsicmp(option, L"FIT") == 0)
							{
								wallStyle = L"6";
							}
							else if (_wcsicmp(option, L"FILL") == 0)
							{
								wallStyle = L"10";
							}
						}

						if (wallStyle)
						{
							RegSetValueEx(hKey, L"WallpaperStyle", 0, REG_SZ, (const BYTE*)wallStyle, sizeof(WCHAR) * 2);
							RegSetValueEx(hKey, L"TileWallpaper", 0, REG_SZ, (const BYTE*)wallTile, sizeof(WCHAR) * 2);
						}
						else
						{
							Log(LOG_ERROR, L"!SetWallpaper: Invalid style");
						}

						RegCloseKey(hKey);
					}
				}

				SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (void*)file.c_str(), SPIF_UPDATEINIFILE);
			}
		}
	}
}

/*
** Copies files and folders from one location to another.
**
*/
bool CSystem::CopyFiles(std::wstring from, std::wstring to, bool bMove)
{
	// The strings must end with double \0
	from.append(1, L'\0');
	to.append(1, L'\0');

	SHFILEOPSTRUCT fo =
	{
		NULL,
		bMove ? FO_MOVE : FO_COPY,
		from.c_str(),
		to.c_str(),
		FOF_NO_UI | FOF_NOCONFIRMATION | FOF_ALLOWUNDO
	};

	int result = SHFileOperation(&fo);
	if (result != 0)
	{
		LogWithArgs(LOG_ERROR, L"Copy error: From %s to %s (%i)", from.c_str(), to.c_str(), result);
		return false;
	}
	return true;
}

/*
** Removes a file even if a file is read-only.
**
*/
bool CSystem::RemoveFile(const std::wstring& file)
{
	DWORD attr = GetFileAttributes(file.c_str());
	if (attr == -1 || (attr & FILE_ATTRIBUTE_READONLY))
	{
		// Unset read-only
		SetFileAttributes(file.c_str(), (attr == -1) ? FILE_ATTRIBUTE_NORMAL : attr - FILE_ATTRIBUTE_READONLY);
	}

	return (DeleteFile(file.c_str()) != 0);
}

/*
** Recursively removes folder.
**
*/
bool CSystem::RemoveFolder(std::wstring folder)
{
	// The strings must end with double nul
	folder.append(1, L'\0');

	SHFILEOPSTRUCT fo =
	{
		NULL,
		FO_DELETE,
		folder.c_str(),
		NULL,
		FOF_NO_UI | FOF_NOCONFIRMATION | FOF_ALLOWUNDO
	};

	int result = SHFileOperation(&fo);
	if (result != 0)
	{
		LogWithArgs(LOG_ERROR, L"Unable to delete folder %s (%i)", folder.c_str(), result);
		return false;
	}
	return true;
}

/*
** Retrieves the "IniFileMapping" entries from Registry.
**
*/
void CSystem::UpdateIniFileMappingList()
{
	static ULONGLONG s_LastWriteTime = 0;

	HKEY hKey;
	LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &hKey);
	if (ret == ERROR_SUCCESS)
	{
		DWORD numSubKeys;
		ULONGLONG ftLastWriteTime;
		bool changed = false;

		ret = RegQueryInfoKey(hKey, NULL, NULL, NULL, &numSubKeys, NULL, NULL, NULL, NULL, NULL, NULL, (LPFILETIME)&ftLastWriteTime);
		if (ret == ERROR_SUCCESS)
		{
			//LogWithArgs(LOG_DEBUG, L"IniFileMapping: numSubKeys=%u, ftLastWriteTime=%llu", numSubKeys, ftLastWriteTime);

			if (ftLastWriteTime != s_LastWriteTime ||
				numSubKeys != c_IniFileMappings.size())
			{
				s_LastWriteTime = ftLastWriteTime;
				if (numSubKeys > c_IniFileMappings.capacity())
				{
					c_IniFileMappings.reserve(numSubKeys);
				}
				changed = true;
			}
		}
		else
		{
			s_LastWriteTime = 0;
			changed = true;
		}

		if (changed)
		{
			if (!c_IniFileMappings.empty())
			{
				c_IniFileMappings.clear();
			}

			WCHAR* buffer = new WCHAR[MAX_PATH];
			DWORD index = 0, cch = MAX_PATH;

			while ((ret = RegEnumKeyEx(hKey, index++, buffer, &cch, NULL, NULL, NULL, NULL)) != ERROR_NO_MORE_ITEMS)
			{
				if (ret == ERROR_SUCCESS)
				{
					c_IniFileMappings.push_back(buffer);
				}
				cch = MAX_PATH;
			}

			delete [] buffer;
		}

		RegCloseKey(hKey);
	}
}

/*
** Prepares a temporary file if iniFile is included in the "IniFileMapping" entries.
** If iniFile is not included, returns a empty string. If error occurred, returns "?".
** Note that a temporary file must be deleted by caller.
**
*/
std::wstring CSystem::GetTemporaryFile(const std::wstring& iniFile)
{
	std::wstring temporary;

	if (!c_IniFileMappings.empty())
	{
		std::wstring::size_type pos = iniFile.find_last_of(L"\\/");
		const WCHAR* filename = iniFile.c_str() + ((pos != std::wstring::npos) ? pos + 1 : 0);

		std::vector<std::wstring>::const_iterator iter = c_IniFileMappings.begin();
		for ( ; iter != c_IniFileMappings.end(); ++iter)
		{
			if (_wcsicmp((*iter).c_str(), filename) == 0)
			{
				WCHAR* buffer = new WCHAR[MAX_PATH];

				if (GetTempPath(MAX_PATH, buffer) != 0 &&
					GetTempFileName(buffer, L"cfg", 0, buffer) != 0)
				{
					temporary = buffer;

					std::wstring tmp = GetTemporaryFile(temporary);
					if (!tmp.empty() || !CopyFiles(iniFile, temporary))  // temporary is reserved or failed
					{
						RemoveFile(temporary);

						if (tmp.empty())
						{
							temporary = L"?";
						}
						else
						{
							temporary.swap(tmp);
						}
					}
				}
				else  // failed
				{
					LogWithArgs(LOG_ERROR, L"Unable to create temporary file to: %s", temporary.c_str());
					temporary = L"?";
				}

				delete [] buffer;
				break;
			}
		}
	}

	return temporary;
}
