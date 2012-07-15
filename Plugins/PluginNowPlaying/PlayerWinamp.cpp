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
#include <cmath>
#include "PlayerWinamp.h"
#include "Winamp/wa_ipc.h"
#include "Winamp/wa_cmd.h"

CPlayer* CPlayerWinamp::c_Player = NULL;

// This player retrieves data through the Winamp IPC interface.

/*
** Constructor.
**
*/
CPlayerWinamp::CPlayerWinamp(WINAMPTYPE type) : CPlayer(),
	m_Window(),
	m_LastCheckTime(0),
	m_UseUnicodeAPI(false),
	m_PlayingStream(false),
	m_WinampType(type),
	m_WinampHandle(),
	m_WinampAddress()
{
}

/*
** Destructor.
**
*/
CPlayerWinamp::~CPlayerWinamp()
{
	c_Player = NULL;
	if (m_WinampHandle) CloseHandle(m_WinampHandle);
}

/*
** Creates a shared class object.
**
*/
CPlayer* CPlayerWinamp::Create(WINAMPTYPE type)
{
	if (!c_Player)
	{
		c_Player = new CPlayerWinamp(type);
	}

	return c_Player;
}

/*
** Try to find Winamp periodically.
**
*/
bool CPlayerWinamp::CheckWindow()
{
	DWORD time = GetTickCount();

	// Try to find Winamp window every 5 seconds
	if (time - m_LastCheckTime > 5000)
	{
		m_LastCheckTime = time;

		m_Window = FindWindow(L"Winamp v1.x", NULL);
		if (m_Window)
		{
			DWORD pID;
			GetWindowThreadProcessId(m_Window, &pID);
			m_WinampHandle = OpenProcess(PROCESS_VM_READ, FALSE, pID);

			if (m_WinampHandle)
			{
				m_WinampAddress = (LPCVOID)SendMessage(m_Window, WM_WA_IPC, 0, IPC_GET_PLAYING_FILENAME);
				m_UseUnicodeAPI = m_WinampAddress ? true : false;
				m_Initialized = true;
			}
		}
	}

	return m_Initialized;
}

/*
** Called during each update of the main measure.
**
*/
void CPlayerWinamp::UpdateData()
{
	if (m_Initialized || CheckWindow())
	{
		int playing = SendMessage(m_Window, WM_WA_IPC, 0, IPC_ISPLAYING);
		if (playing == 0)
		{
			// Make sure Winamp is still active
			if (!IsWindow(m_Window))
			{
				m_Initialized = false;
				ClearData();

				if (m_WinampHandle) CloseHandle(m_WinampHandle);
			}
			else if (m_State != STATE_STOPPED)
			{
				ClearData(false);
			}

			// Don't continue if Winamp has quit or is stopped
			return;
		}
		else
		{
			m_State = (playing == 1) ? STATE_PLAYING : STATE_PAUSED;
			m_Position = SendMessage(m_Window, WM_WA_IPC, 0, IPC_GETOUTPUTTIME) / 1000;		// ms to secs
			m_Volume = (SendMessage(m_Window, WM_WA_IPC, -666, IPC_SETVOLUME) * 100) / 255;	// 0 - 255 to 0 - 100
		}

		WCHAR wBuffer[MAX_PATH];
		char cBuffer[MAX_PATH];

		if (m_UseUnicodeAPI)
		{
			if (!ReadProcessMemory(m_WinampHandle, m_WinampAddress, &wBuffer, sizeof(wBuffer), NULL))
			{
				// Failed to read memory
				return;
			}
		}
		else
		{
			// MediaMonkey doesn't support wide IPC messages
			int pos = SendMessage(m_Window, WM_WA_IPC, 0, IPC_GETLISTPOS);
			LPCVOID address = (LPCVOID)SendMessage(m_Window, WM_WA_IPC, pos, IPC_GETPLAYLISTFILE);

			if (!ReadProcessMemory(m_WinampHandle, address, &cBuffer, sizeof(cBuffer), NULL))
			{
				// Failed to read memory
				return;
			}

			mbstowcs(wBuffer, cBuffer, MAX_PATH);
		}

		if (wcscmp(wBuffer, m_FilePath.c_str()) != 0)
		{
			++m_TrackCount;
			m_FilePath = wBuffer;
			m_PlayingStream = (m_FilePath.find(L"://") != std::wstring::npos);

			if (!m_PlayingStream)
			{
				int duration = SendMessage(m_Window, WM_WA_IPC, 1, IPC_GETOUTPUTTIME);
				m_Duration = (duration != -1) ? duration : 0;

				m_Rating = SendMessage(m_Window, WM_WA_IPC, 0, IPC_GETRATING);
				m_Shuffle = (bool)SendMessage(m_Window, WM_WA_IPC, 0, IPC_GET_SHUFFLE);
				m_Repeat = (bool)SendMessage(m_Window, WM_WA_IPC, 0, IPC_GET_REPEAT);

				TagLib::FileRef fr(wBuffer, false);
				TagLib::Tag* tag = fr.tag();
				if (tag)
				{
					m_Artist = tag->artist().toWString();
					m_Album = tag->album().toWString();
					m_Title = tag->title().toWString();
					m_Number = tag->track();
					m_Year = tag->year();

					if (m_Measures & MEASURE_LYRICS)
					{
						FindLyrics();
					}
				}
				else if (m_Measures & MEASURE_LYRICS)
				{
					m_Lyrics.clear();
				}

				// Find cover if needed
				if (m_Measures & MEASURE_COVER)
				{
					if (tag && CCover::GetEmbedded(fr, m_TempCoverPath))
					{
						// Got everything, return
						m_CoverPath = m_TempCoverPath;
						return;
					}

					std::wstring trackFolder = CCover::GetFileFolder(m_FilePath);
					if (tag && !m_Album.empty())
					{
						// Winamp stores covers usually as %album%.jpg
						std::wstring file = m_Album;
						std::wstring::size_type end = file.length();
						for (std::wstring::size_type pos = 0; pos < end; ++pos)
						{
							// Replace reserved chars according to Winamp specs
							switch (file[pos])
							{
							case L'?':
							case L'*':
							case L'|':
								file[pos] = L'_';
								break;

							case L'/':
							case L'\\':
							case L':':
								file[pos] = L'-';
								break;

							case L'\"':
								file[pos] = L'\'';
								break;

							case L'<':
								file[pos] = L'(';
								break;

							case L'>':
								file[pos] = L')';
								break;
							}
						}

						if (CCover::GetLocal(file, trackFolder, m_CoverPath))
						{
							// %album% art file found
							return;
						}
					}

					if (!CCover::GetLocal(L"cover", trackFolder, m_CoverPath) &&
						!CCover::GetLocal(L"folder", trackFolder, m_CoverPath))
					{
						// Nothing found
						m_CoverPath.clear();
					}
				}

				if (tag)
				{
					return;
				}
			}
			else
			{
				m_Rating = 0;
				m_Duration = 0;
				m_CoverPath.clear();
			}
		}
		else if (!m_PlayingStream)
		{
			if (m_Duration == 0)
			{
				int duration = SendMessage(m_Window, WM_WA_IPC, 1, IPC_GETOUTPUTTIME);
				m_Duration = (duration != -1) ? duration : 0;
			}

			return;
		}

		// TagLib couldn't parse the file or Winamp is playing a stream, try to get title
		if (m_UseUnicodeAPI)
		{
			LPCVOID address = (LPCVOID)SendMessage(m_Window, WM_WA_IPC, 0, IPC_GET_PLAYING_TITLE);
			ReadProcessMemory(m_WinampHandle, address, &wBuffer, sizeof(wBuffer), NULL);
		}
		else
		{
			int pos = SendMessage(m_Window, WM_WA_IPC, 0, IPC_GETLISTPOS);
			LPCVOID address = (LPCVOID)SendMessage(m_Window, WM_WA_IPC, pos, IPC_GETPLAYLISTTITLE);
			ReadProcessMemory(m_WinampHandle, address, &cBuffer, sizeof(cBuffer), NULL);
			mbstowcs(wBuffer, cBuffer, MAX_PATH);
		}

		std::wstring title = wBuffer;
		std::wstring::size_type pos = title.find(L" - ");
		if (pos != std::wstring::npos)
		{
			m_Artist.assign(title, 0, pos);
			pos += 3;  // Skip " - "
			m_Title.assign(title, pos, title.length() - pos);
			m_Album.clear();

			if (m_PlayingStream)
			{
				// Remove crap from title if playing radio
				pos = m_Title.find(L" (");
				if (pos != std::wstring::npos)
				{
					m_Title.resize(pos);
				}
			}
		}
		else
		{
			m_Title = title;
			m_Artist.clear();
			m_Album.clear();
		}
	}
}

/*
** Handles the Pause bang.
**
*/
void CPlayerWinamp::Pause()
{
	SendMessage(m_Window, WM_COMMAND, WINAMP_PAUSE, 0);
}

/*
** Handles the Play bang.
**
*/
void CPlayerWinamp::Play()
{
	SendMessage(m_Window, WM_COMMAND, WINAMP_PLAY, 0);
}

/*
** Handles the Stop bang.
**
*/
void CPlayerWinamp::Stop()
{
	SendMessage(m_Window, WM_COMMAND, WINAMP_STOP, 0);
}

/*
** Handles the Next bang.
**
*/
void CPlayerWinamp::Next() 
{
	SendMessage(m_Window, WM_COMMAND, WINAMP_FASTFWD, 0);
}

/*
** Handles the Previous bang.
**
*/
void CPlayerWinamp::Previous()
{
	SendMessage(m_Window, WM_COMMAND, WINAMP_REWIND, 0);
}

/*
** Handles the SetPosition bang.
**
*/
void CPlayerWinamp::SetPosition(int position)
{
	position *= 1000; // To milliseconds
	SendMessage(m_Window, WM_WA_IPC, position, IPC_JUMPTOTIME);
}

/*
** Handles the SetRating bang.
**
*/
void CPlayerWinamp::SetRating(int rating)
{
	if (rating < 0)
	{
		rating = 0;
	}
	else if (rating > 5)
	{
		rating = 5;
	}

	SendMessage(m_Window, WM_WA_IPC, rating, IPC_SETRATING);
	m_Rating = rating;
}

/*
** Handles the SetVolume bang.
**
*/
void CPlayerWinamp::SetVolume(int volume)
{
	// Winamp accepts volume in 0 - 255 range
	float fVolume = volume * 2.55f;
	SendMessage(m_Window, WM_WA_IPC, (WPARAM)ceil(fVolume), IPC_SETVOLUME);
}

/*
** Handles the SetShuffle bang.
**
*/
void CPlayerWinamp::SetShuffle(bool state)
{
	if (!m_PlayingStream)
	{
		m_Shuffle = state;
		SendMessage(m_Window, WM_WA_IPC, (WPARAM)m_Shuffle, IPC_SET_SHUFFLE);
	}
}

/*
** Handles the SetRepeat bang.
**
*/
void CPlayerWinamp::SetRepeat(bool state)
{
	if (!m_PlayingStream)
	{
		m_Repeat = state;
		SendMessage(m_Window, WM_WA_IPC, (WPARAM)m_Repeat, IPC_SET_REPEAT);
	}
}

/*
** Handles the ClosePlayer bang.
**
*/
void CPlayerWinamp::ClosePlayer()
{
	if (m_WinampType == WA_WINAMP)
	{
		SendMessage(m_Window, WM_CLOSE, 0, 0);
	}
	else // if (m_WinampType == WA_MEDIAMONKEY)
	{
		HWND wnd = FindWindow(L"TFMainWindow", L"MediaMonkey");
		if (wnd)
		{
			SendMessage(wnd, WM_CLOSE, 0, 0);
		}
	}
}

/*
** Handles the OpenPlayer bang.
**
*/
void CPlayerWinamp::OpenPlayer(std::wstring& path)
{
	if (m_WinampType == WA_WINAMP)
	{
		ShellExecute(NULL, L"open", path.empty() ? L"winamp.exe" : path.c_str(), NULL, NULL, SW_SHOW);
	}
	else // if (m_WinampType == WA_MEDIAMONKEY)
	{
		if (path.empty())
		{
			// Gotta figure out where Winamp is located at
			HKEY hKey;
			RegOpenKeyEx(HKEY_LOCAL_MACHINE,
						 L"SOFTWARE\\Clients\\Media\\MediaMonkey\\shell\\open\\command",
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
					ShellExecute(NULL, L"open", data, NULL, NULL, SW_SHOW);
					path = data;
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
}
