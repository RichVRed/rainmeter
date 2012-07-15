/*
  Copyright (C) 2011 Birunthan Mohanathas (www.poiru.net)

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
#include "PlayerSpotify.h"

CPlayer* CPlayerSpotify::c_Player = NULL;

/*
** Constructor.
**
*/
CPlayerSpotify::CPlayerSpotify() : CPlayer(),
	m_Window(),
	m_LastCheckTime(0)
{
}

/*
** Destructor.
**
*/
CPlayerSpotify::~CPlayerSpotify()
{
	c_Player = NULL;
}

/*
** Creates a shared class object.
**
*/
CPlayer* CPlayerSpotify::Create()
{
	if (!c_Player)
	{
		c_Player = new CPlayerSpotify();
	}

	return c_Player;
}

/*
** Try to find Spotify periodically.
**
*/
bool CPlayerSpotify::CheckWindow()
{
	DWORD time = GetTickCount();
		
	// Try to find Spotify window every 5 seconds
	if (time - m_LastCheckTime > 5000)
	{
		m_LastCheckTime = time;

		m_Window = FindWindow(L"SpotifyMainWindow", NULL);
		if (m_Window)
		{
			m_Initialized = true;
		}
	}

	return m_Initialized;
}

/*
** Called during each update of the main measure.
**
*/
void CPlayerSpotify::UpdateData()
{
	if (m_Initialized || CheckWindow())
	{
		// Parse title and artist from window title
		WCHAR buffer[256];
		if (GetWindowText(m_Window, buffer, 256) > 10)
		{
			std::wstring title = &buffer[10];  // Skip "Spotify - "

			std::wstring::size_type pos = title.find(L" � ");
			if (pos != std::wstring::npos)
			{
				std::wstring artist(title, 0, pos);
				pos += 3;  // Skip " - "
				std::wstring track(title, pos);

				if (track != m_Title || artist != m_Artist)
				{
					m_State = STATE_PLAYING;
					m_Title = track;
					m_Artist = artist;
					++m_TrackCount;

					if (m_Measures & MEASURE_LYRICS)
					{
						FindLyrics();
					}
				}
				return;
			}
		}
		else if (IsWindow(m_Window))
		{
			m_State = STATE_PAUSED;
		}
		else
		{
			ClearData();
			m_Initialized = false;
		}
	}
}

/*
** Handles the Play bang.
**
*/
void CPlayerSpotify::Play()
{
	SendMessage(m_Window, WM_APPCOMMAND, 0, SPOTIFY_PLAYPAUSE);
}

/*
** Handles the Stop bang.
**
*/
void CPlayerSpotify::Stop() 
{
	SendMessage(m_Window, WM_APPCOMMAND, 0, SPOTIFY_STOP);
}

/*
** Handles the Next bang.
**
*/
void CPlayerSpotify::Next() 
{
	SendMessage(m_Window, WM_APPCOMMAND, 0, SPOTIFY_NEXT);
}

/*
** Handles the Previous bang.
**
*/
void CPlayerSpotify::Previous() 
{
	SendMessage(m_Window, WM_APPCOMMAND, 0, SPOTIFY_PREV);
}


/*
** Handles the ClosePlayer bang.
**
*/
void CPlayerSpotify::ClosePlayer()
{
	// A little harsh...
	DWORD pID;
	GetWindowThreadProcessId(m_Window, &pID);
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pID);
	if (hProcess)
	{
		TerminateProcess(hProcess, 0);
		CloseHandle(hProcess);
	}
}

/*
** Handles the OpenPlayer bang.
**
*/
void CPlayerSpotify::OpenPlayer(std::wstring& path)
{
	if (!m_Initialized)
	{
		if (path.empty())
		{
			// Gotta figure out where Winamp is located at
			HKEY hKey;
			RegOpenKeyEx(HKEY_CLASSES_ROOT,
						 L"spotify\\DefaultIcon",
						 0,
						 KEY_QUERY_VALUE,
						 &hKey);

			DWORD size = 512;
			WCHAR* data = new WCHAR[size];
			DWORD type = 0;

			if (RegQueryValueEx(hKey,
								NULL,
								NULL,
								(LPDWORD)&type,
								(LPBYTE)data,
								(LPDWORD)&size) == ERROR_SUCCESS)
			{
				if (type == REG_SZ)
				{
					path = data;
					path.erase(0, 1);				// Get rid of the leading quote
					path.resize(path.length() - 3);	// And the ",0 at the end
					ShellExecute(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
				}
			}

			delete [] data;
			RegCloseKey(hKey);
		}
		else
		{
			ShellExecute(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
		}
	}
	else
	{
		// Already active, restore the window
		ShowWindow(m_Window, SW_SHOWNORMAL);
		BringWindowToTop(m_Window);
	}
}
