/*
  Copyright (C) 2005 Kimmo Pekkola

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

#ifndef __METERBUTTON_H__
#define __METERBUTTON_H__

#include "Meter.h"
#include "TintedImage.h"

#define BUTTON_FRAMES 3

class CMeterButton : public CMeter
{
public:
	CMeterButton(CMeterWindow* meterWindow, const WCHAR* name);
	virtual ~CMeterButton();

	virtual UINT GetTypeID() { return TypeID<CMeterButton>(); }

	virtual void Initialize();
	virtual bool Update();
	virtual bool Draw(Gdiplus::Graphics& graphics);
	virtual void BindMeasure(const std::list<CMeasure*>& measures);

	bool MouseMove(POINT pos);
	bool MouseUp(POINT pos, bool execute);
	bool MouseDown(POINT pos);

	void SetFocus(bool f) { m_Focus = f; }

protected:
	virtual void ReadOptions(CConfigParser& parser, const WCHAR* section);

private:
	bool HitTest2(int px, int py, bool checkAlpha);

	CTintedImage m_Image;
	std::wstring m_ImageName;
	bool m_NeedsReload;

	Gdiplus::CachedBitmap* m_Bitmaps[BUTTON_FRAMES];	// Cached bitmaps
	std::wstring m_Command;
	int m_State;
	bool m_Clicked;
	bool m_Focus;
};

#endif
