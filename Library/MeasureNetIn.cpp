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
#include "MeasureNetIn.h"

/*
** The constructor
**
*/
CMeasureNetIn::CMeasureNetIn(CMeterWindow* meterWindow, const WCHAR* name) : CMeasureNet(meterWindow, name),
	m_FirstTime(true),
	m_InOctets()
{
}

/*
** The destructor
**
*/
CMeasureNetIn::~CMeasureNetIn()
{
}

/*
** Updates the current net in value.
**
*/
void CMeasureNetIn::UpdateValue()
{
	if (c_Table == NULL) return;

	if (m_Cumulative)
	{
		m_Value = (double)(__int64)GetNetStatsValue(NET_IN);
	}
	else
	{
		ULONG64 value = 0;

		if (!m_FirstTime)
		{
			value = GetNetOctets(NET_IN);
			if (value > m_InOctets)
			{
				ULONG64 tmpValue = value;
				value -= m_InOctets;
				m_InOctets = tmpValue;
			}
			else
			{
				m_InOctets = value;
				value = 0;
			}
		}
		else
		{
			m_InOctets = GetNetOctets(NET_IN);
			m_FirstTime = false;
		}

		m_Value = (double)(__int64)value;
	}
}

/*
** Read the options specified in the ini file.
**
*/
void CMeasureNetIn::ReadOptions(CConfigParser& parser, const WCHAR* section)
{
	CMeasure::ReadOptions(parser, section);
	CMeasureNet::ReadOptions(parser, section, NET_IN);
}

