/*
  Copyright (C) 2001 Kimmo Pekkola

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

#ifndef __METERWINDOW_H__
#define __METERWINDOW_H__

#include <windows.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <string>
#include <list>
#include "ConfigParser.h"
#include "Group.h"
#include "Mouse.h"

#define BEGIN_MESSAGEPROC switch (uMsg) {
#define MESSAGE(handler, msg) case msg: return window->handler(uMsg, wParam, lParam);
#define REJECT_MESSAGE(msg) case msg: return 0;
#define END_MESSAGEPROC } return DefWindowProc(hWnd, uMsg, wParam, lParam);

#define WM_METERWINDOW_DELAYED_REFRESH WM_APP + 1
#define WM_METERWINDOW_DELAYED_MOVE    WM_APP + 3

#define METERWINDOW_CLASS_NAME	L"RainmeterMeterWindow"

typedef HRESULT (WINAPI * FPDWMENABLEBLURBEHINDWINDOW)(HWND hWnd, const DWM_BLURBEHIND* pBlurBehind);
typedef HRESULT (WINAPI * FPDWMGETCOLORIZATIONCOLOR)(DWORD* pcrColorization, BOOL* pfOpaqueBlend);
typedef HRESULT (WINAPI * FPDWMSETWINDOWATTRIBUTE)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
typedef HRESULT (WINAPI * FPDWMISCOMPOSITIONENABLED)(BOOL* pfEnabled);

enum BUTTONPROC
{
	BUTTONPROC_DOWN,
	BUTTONPROC_UP,
	BUTTONPROC_MOVE
};

enum ZPOSITION
{
	ZPOSITION_ONDESKTOP = -2,
	ZPOSITION_ONBOTTOM = -1,
	ZPOSITION_NORMAL = 0,
	ZPOSITION_ONTOP = 1,
	ZPOSITION_ONTOPMOST = 2
};

enum BLURMODE
{
	BLURMODE_NONE = 0,
	BLURMODE_FULL,
	BLURMODE_REGION
};

enum BGMODE
{
	BGMODE_IMAGE = 0,
	BGMODE_COPY,
	BGMODE_SOLID,
	BGMODE_SCALED_IMAGE,
	BGMODE_TILED_IMAGE
};

enum HIDEMODE
{
	HIDEMODE_NONE = 0,
	HIDEMODE_HIDE,
	HIDEMODE_FADEIN,
	HIDEMODE_FADEOUT
};

enum BEVELTYPE 
{
	BEVELTYPE_NONE,
	BEVELTYPE_UP,
	BEVELTYPE_DOWN
};

enum BANGCOMMAND
{
	BANG_REFRESH,
	BANG_REDRAW,
	BANG_UPDATE,
	BANG_TOGGLEMETER,
	BANG_SHOWMETER,
	BANG_HIDEMETER,
	BANG_MOVEMETER,
	BANG_UPDATEMETER,
	BANG_TOGGLEMEASURE,
	BANG_ENABLEMEASURE,
	BANG_DISABLEMEASURE,
	BANG_UPDATEMEASURE,
	BANG_COMMANDMEASURE,
	BANG_SHOWBLUR,
	BANG_HIDEBLUR,
	BANG_TOGGLEBLUR,
	BANG_ADDBLUR,
	BANG_REMOVEBLUR,
	BANG_SHOW,
	BANG_HIDE,
	BANG_TOGGLE,
	BANG_SHOWFADE,
	BANG_HIDEFADE,
	BANG_TOGGLEFADE,
	BANG_MOVE,
	BANG_ZPOS,
	BANG_SETTRANSPARENCY,
	BANG_CLICKTHROUGH,
	BANG_DRAGGABLE,
	BANG_SNAPEDGES,
	BANG_KEEPONSCREEN,

	BANG_TOGGLEMETERGROUP,
	BANG_SHOWMETERGROUP,
	BANG_HIDEMETERGROUP,
	BANG_UPDATEMETERGROUP,
	BANG_TOGGLEMEASUREGROUP,
	BANG_ENABLEMEASUREGROUP,
	BANG_DISABLEMEASUREGROUP,
	BANG_UPDATEMEASUREGROUP,

	BANG_PLUGIN,
	BANG_SETVARIABLE,
	BANG_SETOPTION,
	BANG_SETOPTIONGROUP
};

class CRainmeter;
class CMeasure;
class CMeter;

class CMeterWindow : public CGroup
{
public:
	CMeterWindow(const std::wstring& folderPath, const std::wstring& file);
	~CMeterWindow();

	int Initialize();

	void RunBang(BANGCOMMAND bang, const std::vector<std::wstring>& args);

	void HideMeter(const std::wstring& name, bool group = false);
	void ShowMeter(const std::wstring& name, bool group = false);
	void ToggleMeter(const std::wstring& name, bool group = false);
	void MoveMeter(const std::wstring& name, int x, int y);
	void UpdateMeter(const std::wstring& name, bool group = false);
	void DisableMeasure(const std::wstring& name, bool group = false);
	void EnableMeasure(const std::wstring& name, bool group = false);
	void ToggleMeasure(const std::wstring& name, bool group = false);
	void UpdateMeasure(const std::wstring& name, bool group = false);
	void Deactivate();
	void Refresh(bool init, bool all = false);
	void Redraw();
	void RedrawWindow() { UpdateWindow(m_TransparencyValue, false); }
	void SetVariable(const std::wstring& variable, const std::wstring& value);
	void SetOption(const std::wstring& section, const std::wstring& option, const std::wstring& value, bool group);

	void SetMouseLeaveEvent(bool cancel);

	void MoveWindow(int x, int y);
	void ChangeZPos(ZPOSITION zPos, bool all = false);
	void ChangeSingleZPos(ZPOSITION zPos, bool all = false);
	void FadeWindow(int from, int to);
	void HideFade();
	void ShowFade();

	void ResizeBlur(const std::wstring& arg, int mode);
	bool IsBlur() { return m_Blur; }
	void SetBlur(bool b) { m_Blur = b; }

	Gdiplus::Bitmap* GetDoubleBuffer() { return m_DoubleBuffer; }
	HWND GetWindow() { return m_Window; }

	CConfigParser& GetParser() { return m_Parser; }

	const std::wstring& GetFolderPath() { return m_FolderPath; }
	const std::wstring& GetFileName() { return m_FileName; }
	std::wstring GetFilePath();
	std::wstring GetRootPath();
	std::wstring GetResourcesPath();

	std::list<CMeasure*>& GetMeasures() { return m_Measures; }
	std::list<CMeter*>& GetMeters() { return m_Meters; }

	ZPOSITION GetWindowZPosition() { return m_WindowZPosition; }
	bool GetXPercentage() { return m_WindowXPercentage; }
	bool GetYPercentage() { return m_WindowYPercentage; }
	bool GetXFromRight() { return m_WindowXFromRight; }
	bool GetYFromBottom() { return m_WindowYFromBottom; }

	int GetW() { return m_WindowW; }
	int GetH() { return m_WindowH; }
	int GetX() { return m_ScreenX; }
	int GetY() { return m_ScreenY; }

	bool GetXScreenDefined() { return m_WindowXScreenDefined; }
	bool GetYScreenDefined() { return m_WindowYScreenDefined; }
	int GetXScreen() { return m_WindowXScreen; }
	int GetYScreen() { return m_WindowYScreen; }

	bool GetClickThrough() { return m_ClickThrough; }
	bool GetKeepOnScreen() { return m_KeepOnScreen; }
	bool GetAutoSelectScreen() { return m_AutoSelectScreen; }
	bool GetWindowDraggable() { return m_WindowDraggable; }
	bool GetSavePosition() { return m_SavePosition; }
	bool GetSnapEdges() { return m_SnapEdges; }
	HIDEMODE GetWindowHide() { return m_WindowHide; }
	int GetAlphaValue() { return m_AlphaValue; }
	int GetUpdateCounter() { return m_UpdateCounter; }
	int GetTransitionUpdate() { return m_TransitionUpdate; }

	bool GetMeterToolTipHidden() { return m_ToolTipHidden; }

	const CMouse& GetMouse() { return m_Mouse; }

	void MakePathAbsolute(std::wstring& path);

	Gdiplus::PrivateFontCollection* GetPrivateFontCollection() { return m_FontCollection; }

	CMeter* GetMeter(const std::wstring& meterName);
	CMeasure* GetMeasure(const std::wstring& measureName) { return m_Parser.GetMeasure(measureName); }

	friend class CDialogManage;

protected:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK InitialWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	LRESULT OnMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCommand(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnEnterSizeMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnExitSizeMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnNcHitTest(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnWindowPosChanging(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnEnterMenuLoop(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnLeftButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnRightButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnMiddleButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnLeftButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnRightButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnMiddleButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnLeftButtonDoubleClick(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnRightButtonDoubleClick(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnMiddleButtonDoubleClick(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDelayedRefresh(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDelayedMove(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDwmColorChange(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDwmCompositionChange(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSettingChange(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDisplayChange(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	enum OPTION
	{
		OPTION_POSITION         = 0x00000001,
		OPTION_ALPHAVALUE       = 0x00000002,
		OPTION_FADEDURATION     = 0x00000004,
		OPTION_CLICKTHROUGH     = 0x00000008,
		OPTION_DRAGGABLE        = 0x00000010,
		OPTION_HIDEONMOUSEOVER  = 0x00000020,
		OPTION_SAVEPOSITION     = 0x00000040,
		OPTION_SNAPEDGES        = 0x00000080,
		OPTION_KEEPONSCREEN     = 0x00000100,
		OPTION_AUTOSELECTSCREEN = 0x00000200,
		OPTION_ALWAYSONTOP      = 0x00000400,

		OPTION_ALL              = 0xFFFFFFFF
	};

	enum RESIZEMODE
	{
		RESIZEMODE_NONE = 0,
		RESIZEMODE_CHECK,
		RESIZEMODE_RESET
	};

	void SetResizeWindowMode(RESIZEMODE mode) { if (m_ResizeWindow != RESIZEMODE_RESET || mode != RESIZEMODE_CHECK) m_ResizeWindow = mode; }

	bool HitTest(int x, int y);

	void SnapToWindow(CMeterWindow* window, LPWINDOWPOS wp);
	void MapCoordsToScreen(int& x, int& y, int w, int h);
	void WindowToScreen();
	void ScreenToWindow();
	void PostUpdate(bool bActiveTransition);
	bool UpdateMeasure(CMeasure* measure, bool force);
	bool UpdateMeter(CMeter* meter, bool& bActiveTransition, bool force);
	void Update(bool refresh);
	void UpdateWindow(int alpha, bool reset);
	void ReadOptions();
	void WriteOptions(INT setting = OPTION_ALL);
	bool ReadSkin();
	void InitializeMeasures();
	void InitializeMeters();
	void ShowWindowIfAppropriate();
	HWND GetWindowFromPoint(POINT pos);
	void HandleButtons(POINT pos, BUTTONPROC proc, bool execute = true);
	void SetClickThrough(bool b);
	void SetKeepOnScreen(bool b);
	void SetWindowDraggable(bool b);
	void SetSavePosition(bool b);
	void SetSnapEdges(bool b);
	void SetWindowHide(HIDEMODE hide);
	void SetWindowZPosition(ZPOSITION zpos);
	bool DoAction(int x, int y, MOUSEACTION action, bool test);
	bool DoMoveAction(int x, int y, MOUSEACTION action);
	bool ResizeWindow(bool reset);
	void IgnoreAeroPeek();
	void AddWindowExStyle(LONG_PTR flag);
	void RemoveWindowExStyle(LONG_PTR flag);
	void BlurBehindWindow(BOOL fEnable);
	void SetWindowPositionVariables(int x, int y);
	void SetWindowSizeVariables(int w, int h);

	void ShowBlur();
	void HideBlur();

	void CreateDoubleBuffer(int cx, int cy);

	CConfigParser m_Parser;

	Gdiplus::Bitmap* m_DoubleBuffer;
	HBITMAP m_DIBSectionBuffer;
	LPDWORD m_DIBSectionBufferPixels;
	int m_DIBSectionBufferW;
	int m_DIBSectionBufferH;

	Gdiplus::Bitmap* m_Background;
	SIZE m_BackgroundSize;

	HWND m_Window;

	CMouse m_Mouse;
	bool m_MouseOver;

	std::wstring m_OnRefreshAction;
	std::wstring m_OnCloseAction;

	std::wstring m_BackgroundName;
	RECT m_BackgroundMargins;
	RECT m_DragMargins;
	std::wstring m_WindowX;
	std::wstring m_WindowY;
	std::wstring m_AnchorX;
	std::wstring m_AnchorY;
	int m_WindowXScreen;
	int m_WindowYScreen;
	bool m_WindowXScreenDefined;
	bool m_WindowYScreenDefined;
	bool m_WindowXFromRight;
	bool m_WindowYFromBottom;
	bool m_WindowXPercentage;
	bool m_WindowYPercentage;
	int m_WindowW;
	int m_WindowH;
	int m_ScreenX;								// X-postion on the virtual screen 
	int m_ScreenY;								// Y-postion on the virtual screen
	bool m_AnchorXFromRight;
	bool m_AnchorYFromBottom;
	bool m_AnchorXPercentage;
	bool m_AnchorYPercentage;
	int m_AnchorScreenX;
	int m_AnchorScreenY;
	bool m_WindowDraggable;
	int m_WindowUpdate;
	int m_TransitionUpdate;
	bool m_ActiveTransition;
	bool m_HasNetMeasures;
	bool m_HasButtons;
	HIDEMODE m_WindowHide;
	bool m_WindowStartHidden;
	bool m_SavePosition;
	bool m_SnapEdges;
	int m_AlphaValue;
	int m_FadeDuration;	
	ZPOSITION m_WindowZPosition;
	bool m_DynamicWindowSize;
	bool m_ClickThrough;
	bool m_KeepOnScreen;
	bool m_AutoSelectScreen;
	bool m_Dragging;
	bool m_Dragged;
	BGMODE m_BackgroundMode;
	Gdiplus::Color m_SolidColor;
	Gdiplus::Color m_SolidColor2;
	Gdiplus::REAL m_SolidAngle;
	BEVELTYPE m_SolidBevel;

	bool m_Blur;
	BLURMODE m_BlurMode;
	HRGN m_BlurRegion;

	ULONGLONG m_FadeStartTime;
	int m_FadeStartValue;
	int m_FadeEndValue;
	int m_TransparencyValue;

	bool m_Refreshing;

	bool m_Hidden;
	RESIZEMODE m_ResizeWindow;

	std::list<CMeasure*> m_Measures;
	std::list<CMeter*> m_Meters;

	const std::wstring m_FolderPath;
	const std::wstring m_FileName;

	int m_UpdateCounter;
	UINT m_MouseMoveCounter;

	Gdiplus::PrivateFontCollection* m_FontCollection;

	bool m_ToolTipHidden;

	static int c_InstanceCount;

	static HINSTANCE c_DwmInstance;

	static FPDWMENABLEBLURBEHINDWINDOW c_DwmEnableBlurBehindWindow;
	static FPDWMGETCOLORIZATIONCOLOR c_DwmGetColorizationColor;
	static FPDWMSETWINDOWATTRIBUTE c_DwmSetWindowAttribute;
	static FPDWMISCOMPOSITIONENABLED c_DwmIsCompositionEnabled;
};

#endif
