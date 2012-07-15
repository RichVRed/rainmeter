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

#include "StdAfx.h"
#include "MeterHistogram.h"
#include "Measure.h"
#include "Error.h"
#include "Rainmeter.h"

using namespace Gdiplus;

extern CRainmeter* Rainmeter;

CTintedImageHelper_DefineOptionArray(CMeterHistogram::c_PrimaryOptionArray, L"Primary");
CTintedImageHelper_DefineOptionArray(CMeterHistogram::c_SecondaryOptionArray, L"Secondary");
CTintedImageHelper_DefineOptionArray(CMeterHistogram::c_BothOptionArray, L"Both");

/*
** The constructor
**
*/
CMeterHistogram::CMeterHistogram(CMeterWindow* meterWindow, const WCHAR* name) : CMeter(meterWindow, name),
	m_SecondaryMeasure(),
	m_PrimaryColor(Color::Green),
	m_SecondaryColor(Color::Red),
	m_OverlapColor(Color::Yellow),
	m_MeterPos(),
	m_Autoscale(false),
	m_Flip(false),
	m_PrimaryImage(L"PrimaryImage", c_PrimaryOptionArray),
	m_SecondaryImage(L"SecondaryImage", c_SecondaryOptionArray),
	m_OverlapImage(L"BothImage", c_BothOptionArray),
	m_PrimaryNeedsReload(false),
	m_SecondaryNeedsReload(false),
	m_OverlapNeedsReload(false),
	m_PrimaryValues(),
	m_SecondaryValues(),
	m_MaxPrimaryValue(1.0),
	m_MinPrimaryValue(),
	m_MaxSecondaryValue(1.0),
	m_MinSecondaryValue(),
	m_SizeChanged(true),
	m_GraphStartLeft(false),
	m_GraphHorizontalOrientation(false)
{
}

/*
** The destructor
**
*/
CMeterHistogram::~CMeterHistogram()
{
	DisposeBuffer();
}

/*
** Disposes the buffers.
**
*/
void CMeterHistogram::DisposeBuffer()
{
	// Reset current position
	m_MeterPos = 0;

	// Delete buffers
	delete [] m_PrimaryValues;
	m_PrimaryValues = NULL;

	delete [] m_SecondaryValues;
	m_SecondaryValues = NULL;
}

/*
** Load the images and calculate the dimensions of the meter from them.
** Or create the brushes if solid color histogram is used.
**
*/
void CMeterHistogram::Initialize()
{
	CMeter::Initialize();

	// A sanity check
	if (m_SecondaryMeasure && !m_PrimaryImageName.empty() && (m_OverlapImageName.empty() || m_SecondaryImageName.empty()))
	{
		Log(LOG_WARNING, L"Histogram: SecondaryImage and BothImage not defined");

		m_PrimaryImage.DisposeImage();
		m_SecondaryImage.DisposeImage();
		m_OverlapImage.DisposeImage();
	}
	else
	{
		// Load the bitmaps if defined
		if (!m_PrimaryImageName.empty())
		{
			m_PrimaryImage.LoadImage(m_PrimaryImageName, m_PrimaryNeedsReload);

			if (m_PrimaryImage.IsLoaded())
			{
				int oldW = m_W;
				int oldH = m_H;

				Bitmap* bitmap = m_PrimaryImage.GetImage();

				m_W = bitmap->GetWidth();
				m_H = bitmap->GetHeight();

				if (oldW != m_W || oldH != m_H)
				{
					m_SizeChanged = true;
				}
			}
		}
		else if (m_PrimaryImage.IsLoaded())
		{
			m_PrimaryImage.DisposeImage();
		}

		if (!m_SecondaryImageName.empty())
		{
			m_SecondaryImage.LoadImage(m_SecondaryImageName, m_SecondaryNeedsReload);
		}
		else if (m_SecondaryImage.IsLoaded())
		{
			m_SecondaryImage.DisposeImage();
		}

		if (!m_OverlapImageName.empty())
		{
			m_OverlapImage.LoadImage(m_OverlapImageName, m_OverlapNeedsReload);
		}
		else if (m_OverlapImage.IsLoaded())
		{
			m_OverlapImage.DisposeImage();
		}
	}

	if ((!m_PrimaryImageName.empty() && !m_PrimaryImage.IsLoaded()) ||
		(!m_SecondaryImageName.empty() && !m_SecondaryImage.IsLoaded()) ||
		(!m_OverlapImageName.empty() && !m_OverlapImage.IsLoaded()))
	{
		DisposeBuffer();

		m_SizeChanged = false;
	}
	else if (m_SizeChanged)
	{
		DisposeBuffer();

		// Create buffers for values
		if (m_W > 0 || m_H > 0)
		{
			int maxSize = m_GraphHorizontalOrientation ? m_H : m_W;
			m_PrimaryValues = new double[maxSize]();
			if (m_SecondaryMeasure)
			{
				m_SecondaryValues = new double[maxSize]();
			}
		}

		m_SizeChanged = false;
	}
}

/*
** Read the options specified in the ini file.
**
*/
void CMeterHistogram::ReadOptions(CConfigParser& parser, const WCHAR* section)
{
	// Store the current values so we know if the image needs to be updated
	std::wstring oldPrimaryImageName = m_PrimaryImageName;
	std::wstring oldSecondaryImageName = m_SecondaryImageName;
	std::wstring oldBothImageName = m_OverlapImageName;
	int oldW = m_W;
	int oldH = m_H;

	CMeter::ReadOptions(parser, section);

	m_PrimaryColor = parser.ReadColor(section, L"PrimaryColor", Color::Green);
	m_SecondaryColor = parser.ReadColor(section, L"SecondaryColor", Color::Red);
	m_OverlapColor = parser.ReadColor(section, L"BothColor", Color::Yellow);

	if (!m_Initialized && !m_MeasureName.empty())
	{
		m_SecondaryMeasureName = parser.ReadString(section, L"MeasureName2", L"");
		if (m_SecondaryMeasureName.empty())
		{
			m_SecondaryMeasureName = parser.ReadString(section, L"SecondaryMeasureName", L"");
		}
	}

	m_PrimaryImageName = parser.ReadString(section, L"PrimaryImage", L"");
	if (!m_PrimaryImageName.empty())
	{
		m_MeterWindow->MakePathAbsolute(m_PrimaryImageName);

		// Read tinting options
		m_PrimaryImage.ReadOptions(parser, section);
	}
	else
	{
		m_PrimaryImage.ClearOptionFlags();
	}

	m_SecondaryImageName = parser.ReadString(section, L"SecondaryImage", L"");
	if (!m_SecondaryImageName.empty())
	{
		m_MeterWindow->MakePathAbsolute(m_SecondaryImageName);

		// Read tinting options
		m_SecondaryImage.ReadOptions(parser, section);
	}
	else
	{
		m_SecondaryImage.ClearOptionFlags();
	}

	m_OverlapImageName = parser.ReadString(section, L"BothImage", L"");
	if (!m_OverlapImageName.empty())
	{
		m_MeterWindow->MakePathAbsolute(m_OverlapImageName);

		// Read tinting options
		m_OverlapImage.ReadOptions(parser, section);
	}
	else
	{
		m_OverlapImage.ClearOptionFlags();
	}

	m_Autoscale = 0!=parser.ReadInt(section, L"AutoScale", 0);
	m_Flip = 0!=parser.ReadInt(section, L"Flip", 0);

	if (m_Initialized)
	{
		if (m_PrimaryImageName.empty())
		{
			if (oldW != m_W || oldH != m_H)
			{
				m_SizeChanged = true;
				Initialize();  // Reload the image
			}
		}
		else
		{
			// Reset to old dimensions
			m_W = oldW;
			m_H = oldH;

			m_PrimaryNeedsReload = (wcscmp(oldPrimaryImageName.c_str(), m_PrimaryImageName.c_str()) != 0);
			m_SecondaryNeedsReload = (wcscmp(oldSecondaryImageName.c_str(), m_SecondaryImageName.c_str()) != 0);
			m_OverlapNeedsReload = (wcscmp(oldBothImageName.c_str(), m_OverlapImageName.c_str()) != 0);

			if (m_PrimaryNeedsReload ||
				m_SecondaryNeedsReload ||
				m_OverlapNeedsReload ||
				m_PrimaryImage.IsOptionsChanged() ||
				m_SecondaryImage.IsOptionsChanged() ||
				m_OverlapImage.IsOptionsChanged())
			{
				Initialize();  // Reload the image
			}
		}
	}

	const WCHAR* graph = parser.ReadString(section, L"GraphStart", L"RIGHT").c_str();
	if (_wcsicmp(graph, L"RIGHT") == 0)
	{
		m_GraphStartLeft = false;
	}
	else if (_wcsicmp(graph, L"LEFT") ==  0)
	{
		m_GraphStartLeft = true;
	}
	else
	{
		LogWithArgs(LOG_ERROR, L"GraphStart=%s is not valid in [%s]", graph, m_Name.c_str());
	}

	graph = parser.ReadString(section, L"GraphOrientation", L"VERTICAL").c_str();
	if (_wcsicmp(graph, L"VERTICAL") == 0)
	{
		// Restart graph
		if (m_GraphHorizontalOrientation)
		{
			m_GraphHorizontalOrientation = false;
			DisposeBuffer();

			// Create buffers for values
			if (m_W > 0)
			{
				m_PrimaryValues = new double[m_W]();
				if (m_SecondaryMeasure)
				{
					m_SecondaryValues = new double[m_W]();
				}
			}
		}
		else
		{
			m_GraphHorizontalOrientation = false;
		}
	}
	else if (_wcsicmp(graph, L"HORIZONTAL") ==  0)
	{
		// Restart graph
		if (!m_GraphHorizontalOrientation)
		{
			m_GraphHorizontalOrientation = true;
			DisposeBuffer();

			// Create buffers for values
			if (m_H > 0)
			{
				m_PrimaryValues = new double[m_H]();
				if (m_SecondaryMeasure)
				{
					m_SecondaryValues = new double[m_H]();
				}
			}
		}
		else
		{
			m_GraphHorizontalOrientation = true;
		}
	}
	else
	{
		LogWithArgs(LOG_ERROR, L"GraphOrientation=%s is not valid in [%s]", graph, m_Name.c_str());
	}
}

/*
** Updates the value(s) from the measures.
**
*/
bool CMeterHistogram::Update()
{
	if (CMeter::Update() && m_Measure && m_PrimaryValues)
	{
		// Gather values
		m_PrimaryValues[m_MeterPos] = m_Measure->GetValue();

		if (m_SecondaryMeasure && m_SecondaryValues)
		{
			m_SecondaryValues[m_MeterPos] = m_SecondaryMeasure->GetValue();
		}

		++m_MeterPos;
		int maxSize = m_GraphHorizontalOrientation ? m_H : m_W;
		m_MeterPos %= maxSize;
		
		m_MaxPrimaryValue = m_Measure->GetMaxValue();
		m_MinPrimaryValue = m_Measure->GetMinValue();
		m_MaxSecondaryValue = 0.0;
		m_MinSecondaryValue = 0.0;
		if (m_SecondaryMeasure)
		{
			m_MaxSecondaryValue = m_SecondaryMeasure->GetMaxValue();
			m_MinSecondaryValue = m_SecondaryMeasure->GetMinValue();
		}

		if (m_Autoscale)
		{
			// Go through all values and find the max

			double newValue = 0.0;
			for (int i = 0; i < maxSize; ++i)
			{
				newValue = max(newValue, m_PrimaryValues[i]);
			}

			// Scale the value up to nearest power of 2
			if (newValue > DBL_MAX / 2.0)
			{
				m_MaxPrimaryValue = DBL_MAX;
			}
			else
			{
				m_MaxPrimaryValue = 2.0;
				while (m_MaxPrimaryValue < newValue)
				{
					m_MaxPrimaryValue *= 2.0;
				}
			}

			if (m_SecondaryMeasure && m_SecondaryValues)
			{
				for (int i = 0; i < maxSize; ++i)
				{
					newValue = max(newValue, m_SecondaryValues[i]);
				}

				// Scale the value up to nearest power of 2
				if (newValue > DBL_MAX / 2.0)
				{
					m_MaxSecondaryValue = DBL_MAX;
				}
				else
				{
					m_MaxSecondaryValue = 2.0;
					while (m_MaxSecondaryValue < newValue)
					{
						m_MaxSecondaryValue *= 2.0;
					}
				}
			}
		}
		return true;
	}
	return false;
}

/*
** Draws the meter on the double buffer
**
*/
bool CMeterHistogram::Draw(Graphics& graphics)
{
	if (!CMeter::Draw(graphics) ||
		(m_Measure && !m_PrimaryValues) ||
		(m_SecondaryMeasure && !m_SecondaryValues)) return false;

	GraphicsPath primaryPath;
	GraphicsPath secondaryPath;
	GraphicsPath bothPath;

	Bitmap* primaryBitmap = m_PrimaryImage.GetImage();
	Bitmap* secondaryBitmap = m_SecondaryImage.GetImage();
	Bitmap* bothBitmap = m_OverlapImage.GetImage();

	int x = GetX();
	int y = GetY();

	// Default values (GraphStart=Right, GraphOrientation=Vertical)
	int i;
	int startValue = 0;
	int* endValueLHS = &i;
	int* endValueRHS = &m_W;
	int step = 1;
	int endValue = -1; //(should be 0, but need to simulate <=)

	// GraphStart=Left, GraphOrientation=Vertical
	if (m_GraphStartLeft && !m_GraphHorizontalOrientation)
	{
		startValue = m_W - 1;
		endValueLHS = &endValue;
		endValueRHS = &i;
		step = -1;
	}
	else if (m_GraphHorizontalOrientation && !m_Flip)
	{
		endValueRHS = &m_H;
	}
	else if (m_GraphHorizontalOrientation && m_Flip)
	{
		startValue = m_H - 1;
		endValueLHS = &endValue;
		endValueRHS = &i;
		step = -1;
	}

	// Horizontal or Vertical graph
	if (m_GraphHorizontalOrientation)
	{
		for (i = startValue; *endValueLHS < *endValueRHS; i += step)
		{
			double value = (m_MaxPrimaryValue == 0.0) ?
				  0.0
				: m_PrimaryValues[(i + (m_MeterPos % m_H)) % m_H] / m_MaxPrimaryValue;
			value -= m_MinPrimaryValue;
			int primaryBarHeight = (int)(m_W * value);
			primaryBarHeight = min(m_W, primaryBarHeight);
			primaryBarHeight = max(0, primaryBarHeight);

			if (m_SecondaryMeasure)
			{
				value = (m_MaxSecondaryValue == 0.0) ?
					  0.0
					: m_SecondaryValues[(i + m_MeterPos) % m_H] / m_MaxSecondaryValue;
				value -= m_MinSecondaryValue;
				int secondaryBarHeight = (int)(m_W * value);
				secondaryBarHeight = min(m_W, secondaryBarHeight);
				secondaryBarHeight = max(0, secondaryBarHeight);

				// Check which measured value is higher
				int bothBarHeight = min(primaryBarHeight, secondaryBarHeight);

				// Cache image/color rectangle for the both lines
				{
					Rect& r = m_GraphStartLeft ?
						  Rect(x, y + startValue + (step * i), bothBarHeight, 1)
						: Rect(x + m_W - bothBarHeight, y + startValue + (step * i), bothBarHeight, 1);

					bothPath.AddRectangle(r);  // cache
				}

				// Cache the image/color rectangle for the rest
				if (secondaryBarHeight > primaryBarHeight)
				{
					Rect& r = m_GraphStartLeft ?
						  Rect(x + bothBarHeight, y + startValue + (step * i), secondaryBarHeight - bothBarHeight, 1)
						: Rect(x + m_W - secondaryBarHeight, y + startValue + (step * i), secondaryBarHeight - bothBarHeight, 1);

					secondaryPath.AddRectangle(r);  // cache
				}
				else
				{
					Rect& r = m_GraphStartLeft ?
						  Rect(x + bothBarHeight, y + startValue + (step * i), primaryBarHeight - bothBarHeight, 1)
						: Rect(x + m_W - primaryBarHeight, y + startValue + (step * i), primaryBarHeight - bothBarHeight, 1);

					primaryPath.AddRectangle(r);  // cache
				}
			}
			else
			{
				Rect& r = m_GraphStartLeft ?
					  Rect(x, y + startValue + (step * i), primaryBarHeight, 1)
					: Rect(x + m_W - primaryBarHeight, y + startValue + (step * i), primaryBarHeight, 1);

				primaryPath.AddRectangle(r);  // cache
			}
		}
	}
	else	// GraphOrientation=Vertical
	{
		for (i = startValue; *endValueLHS < *endValueRHS; i += step)
		{
			double value = (m_MaxPrimaryValue == 0.0) ?
				  0.0
				: m_PrimaryValues[(i + m_MeterPos) % m_W] / m_MaxPrimaryValue;
			value -= m_MinPrimaryValue;
			int primaryBarHeight = (int)(m_H * value);
			primaryBarHeight = min(m_H, primaryBarHeight);
			primaryBarHeight = max(0, primaryBarHeight);

			if (m_SecondaryMeasure)
			{
				value = (m_MaxSecondaryValue == 0.0) ?
					  0.0
					: m_SecondaryValues[(i + m_MeterPos) % m_W] / m_MaxSecondaryValue;
				value -= m_MinSecondaryValue;
				int secondaryBarHeight = (int)(m_H * value);
				secondaryBarHeight = min(m_H, secondaryBarHeight);
				secondaryBarHeight = max(0, secondaryBarHeight);

				// Check which measured value is higher
				int bothBarHeight = min(primaryBarHeight, secondaryBarHeight);

				// Cache image/color rectangle for the both lines
				{
					Rect& r = m_Flip ?
						  Rect(x + startValue + (step * i), y, 1, bothBarHeight)
						: Rect(x + startValue + (step * i), y + m_H - bothBarHeight, 1, bothBarHeight);

					bothPath.AddRectangle(r);  // cache
				}

				// Cache the image/color rectangle for the rest
				if (secondaryBarHeight > primaryBarHeight)
				{
					Rect& r = m_Flip ?
						  Rect(x + startValue + (step * i), y + bothBarHeight, 1, secondaryBarHeight - bothBarHeight)
						: Rect(x + startValue + (step * i), y + m_H - secondaryBarHeight, 1, secondaryBarHeight - bothBarHeight);

					secondaryPath.AddRectangle(r);  // cache
				}
				else
				{
					Rect& r = m_Flip ?
						  Rect(x + startValue + (step * i), y + bothBarHeight, 1, primaryBarHeight - bothBarHeight)
						: Rect(x + startValue + (step * i), y + m_H - primaryBarHeight, 1, primaryBarHeight - bothBarHeight);

					primaryPath.AddRectangle(r);  // cache
				}
			}
			else
			{
				Rect& r = m_Flip ?
					  Rect(x + startValue + (step * i), y, 1, primaryBarHeight)
					: Rect(x + startValue + (step * i), y + m_H - primaryBarHeight, 1, primaryBarHeight);

				primaryPath.AddRectangle(r);  // cache
			}
		}
	}

	// Draw cached rectangles
	if (primaryBitmap)
	{
		Rect r(x, y, primaryBitmap->GetWidth(), primaryBitmap->GetHeight());

		graphics.SetClip(&primaryPath);
		graphics.DrawImage(primaryBitmap, r, 0, 0, r.Width, r.Height, UnitPixel);
		graphics.ResetClip();
	}
	else
	{
		SolidBrush brush(m_PrimaryColor);
		graphics.FillPath(&brush, &primaryPath);
	}
	if (m_SecondaryMeasure)
	{
		if (secondaryBitmap)
		{
			Rect r(x, y, secondaryBitmap->GetWidth(), secondaryBitmap->GetHeight());

			graphics.SetClip(&secondaryPath);
			graphics.DrawImage(secondaryBitmap, r, 0, 0, r.Width, r.Height, UnitPixel);
			graphics.ResetClip();
		}
		else
		{
			SolidBrush brush(m_SecondaryColor);
			graphics.FillPath(&brush, &secondaryPath);
		}
		if (bothBitmap)
		{
			Rect r(x, y, bothBitmap->GetWidth(), bothBitmap->GetHeight());

			graphics.SetClip(&bothPath);
			graphics.DrawImage(bothBitmap, r, 0, 0, r.Width, r.Height, UnitPixel);
			graphics.ResetClip();
		}
		else
		{
			SolidBrush brush(m_OverlapColor);
			graphics.FillPath(&brush, &bothPath);
		}
	}

	return true;
}

/*
** Overwritten method to handle the secondary measure binding.
**
*/
void CMeterHistogram::BindMeasure(const std::list<CMeasure*>& measures)
{
	CMeter::BindMeasure(measures);

	if (!m_SecondaryMeasureName.empty())
	{
		// Go through the list and check it there is a secondary measure for us
		const WCHAR* name = m_SecondaryMeasureName.c_str();
		std::list<CMeasure*>::const_iterator i = measures.begin();
		for ( ; i != measures.end(); ++i)
		{
			if (_wcsicmp((*i)->GetName(), name) == 0)
			{
				m_SecondaryMeasure = (*i);
				CMeter::SetAllMeasures(m_SecondaryMeasure);
				return;
			}
		}

		std::wstring error = L"The meter [" + m_Name;
		error += L"] cannot be bound with [";
		error += m_SecondaryMeasureName;
		error += L']';
		throw CError(error);
	}
}
