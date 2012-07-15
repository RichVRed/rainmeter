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
#include "MeasureNetOut.h"

/*
** The constructor
**
*/
CMeasureNetOut::CMeasureNetOut(CMeterWindow* meterWindow, const WCHAR* name) : CMeasureNet(meterWindow, name),
	m_FirstTime(true),
	m_OutOctets()
{
}

/*
** The destructor
**
*/
CMeasureNetOut::~CMeasureNetOut()
{
}

/*
** Updates the current net out value.
**
*/
void CMeasureNetOut::UpdateValue()
{
	if (c_Table == NULL) return;

	if (m_Cumulative)
	{
		m_Value = (double)(__int64)GetNetStatsValue(NET_OUT);
	}
	else
	{
		ULONG64 value = 0;

		if (!m_FirstTime)
		{
			value = GetNetOctets(NET_OUT);
			if (value > m_OutOctets)
			{
				ULONG64 tmpValue = value;
				value -= m_OutOctets;
				m_OutOctets = tmpValue;
			}
			else
			{
				m_OutOctets = value;
				value = 0;
			}
		}
		else
		{
			m_OutOctets = GetNetOctets(NET_OUT);
			m_FirstTime = false;
		}

		m_Value = (double)(__int64)value;
	}
}

/*
** Read the options specified in the ini file.
**
*/
void CMeasureNetOut::ReadOptions(CConfigParser& parser, const WCHAR* section)
{
	CMeasure::ReadOptions(parser, section);
	CMeasureNet::ReadOptions(parser, section, NET_OUT);
}
