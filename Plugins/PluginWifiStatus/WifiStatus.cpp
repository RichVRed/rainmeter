/*
  Copyright (C) 2009 Shaivya Mahajan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <string>
#include <wlanapi.h>
#include "../API/RainmeterAPI.h"
#include "../../Library/DisableThreadLibraryCalls.h"	// contains DllMain entry point

enum MEASURETYPE
{
	UNINITIALIZED,
	UNKNOWN,
	SSID,
	QUALITY,
	ENCRYPTION,
	AUTH,
	LIST,
	PHY
};

struct MeasureData
{
	MEASURETYPE type;
	UINT listStyle;
	UINT listMax;
	std::wstring statusString;

	MeasureData() : type(UNINITIALIZED), listStyle(), listMax(5) {}
};

UINT g_Instances = 0;

// Globals that store system's wifi interface/adapter structs
// These are initialized in Initialize(), used during each update
HANDLE g_hClient = NULL;
PWLAN_INTERFACE_INFO g_pInterface = NULL;
PWLAN_INTERFACE_INFO_LIST g_pIntfList = NULL;

// Function that translates DOT11 ENUMs to output strings
LPCWSTR GetDot11Str(int, int);

std::wstring ConvertToWide(LPCSTR str, int strLen)
{
	std::wstring szWide;

	if (str && *str)
	{
		int bufLen = MultiByteToWideChar(CP_ACP, 0, str, strLen, NULL, 0);
		if (bufLen > 0)
		{
			szWide.resize(bufLen);
			MultiByteToWideChar(CP_ACP, 0, str, strLen, &szWide[0], bufLen);
		}
	}
	return szWide;
}

void FinalizeHandle()
{
	g_pInterface = NULL;

	if (g_pIntfList != NULL)
	{
		WlanFreeMemory(g_pIntfList);
		g_pIntfList = NULL;
	}

	if (g_hClient != NULL)
	{
		WlanCloseHandle(g_hClient, NULL);
		g_hClient = NULL;
	}
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	MeasureData* measure = new MeasureData;
	*data = measure;

	++g_Instances;

	if (g_Instances == 1)
	{
		WCHAR buffer[256];

		// Create WINLAN API Handle
		if (g_hClient == NULL)
		{
			DWORD dwNegotiatedVersion = 0;
			DWORD dwErr = WlanOpenHandle(WLAN_API_VERSION, NULL, &dwNegotiatedVersion, &g_hClient);
			if (ERROR_SUCCESS != dwErr)
			{
				FinalizeHandle();
				_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: Unable to open WLAN API Handle. Error code (%u): %s", dwErr, GetDot11Str((int)dwErr, 5));
				RmLog(LOG_ERROR, buffer);
				return;
			}
		}

		// Query list of WLAN interfaces
		if (g_pIntfList == NULL)
		{
			DWORD dwErr = WlanEnumInterfaces(g_hClient, NULL, &g_pIntfList);
			if (ERROR_SUCCESS != dwErr)
			{
				FinalizeHandle();
				_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: Unable to find any WLAN interfaces/adapters. Error code %u", dwErr);
				RmLog(LOG_ERROR, buffer);
				return;
			}
			else if (g_pIntfList->dwNumberOfItems == 0)
			{
				FinalizeHandle();
				RmLog(LOG_ERROR, L"WifiStatus.dll: No WLAN interfaces/adapters available.");
				return;
			}
		}
	}
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	if (g_hClient == NULL) return;

	MeasureData* measure = (MeasureData*)data;
	WCHAR buffer[256];
	bool changed = false;

	// Select a WLAN interface, default 0.
	int value = RmReadInt(rm, L"WifiIntfID", 0);
	if (value >= (int)g_pIntfList->dwNumberOfItems)
	{
		_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: Adapter (WifiIntfID=%i) not valid.", value);
		RmLog(LOG_ERROR, buffer);

		value = 0;
	}
	g_pInterface = &g_pIntfList->InterfaceInfo[value];

	// Select LIST style
	value = RmReadInt(rm, L"WifiListStyle", 0);
	if (value < 0 || value > 3)
	{
		_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: WifiListStyle=%i not valid.", value);
		RmLog(LOG_WARNING, buffer);

		value = 0;
	}
	measure->listStyle = value;

	// Set maxmimum number of list items
	value = RmReadInt(rm, L"WifiListLimit", 5);
	if (value <= 0)
	{
		_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: WifiListLimit=%i not valid.", data);
		RmLog(LOG_WARNING, buffer);

		value = 5;
	}
	measure->listMax = value;

	// Select type of measure
	MEASURETYPE infoType = UNKNOWN;
	LPCWSTR type = RmReadString(rm, L"WifiInfoType", L"");
	if (_wcsicmp(L"SSID", type) == 0)
	{
		infoType = SSID;
	}
	else if (_wcsicmp(L"QUALITY", type) == 0)
	{
		infoType = QUALITY;
	}
	else if (_wcsicmp(L"ENCRYPTION", type) == 0)
	{
		infoType = ENCRYPTION;
	}
	else if (_wcsicmp(L"AUTH", type) == 0)
	{
		infoType = AUTH;
	}
	else if (_wcsicmp(L"LIST", type) == 0)
	{
		infoType = LIST;
	}
	else if (_wcsicmp(L"PHY", type) == 0)
	{
		infoType = PHY;
	}
	else
	{
		_snwprintf_s(buffer, _TRUNCATE, L"WifiStatus.dll: WifiInfoType=%s not valid.", type);
		RmLog(LOG_ERROR, buffer);
	}
	if (infoType != measure->type)
	{
		changed = true;
	}
	measure->type = infoType;

	if (changed)
	{
		measure->statusString.clear();

		switch (infoType)
		{
		case SSID:
		case ENCRYPTION:
		case AUTH:
			*maxValue = 0;
			break;
		case QUALITY:
			*maxValue = 100;
			break;
		}
	}
}

PLUGIN_EXPORT double Update(void* data)
{
	if (g_pInterface == NULL) return 0;

	MeasureData* measure = (MeasureData*)data;
	double value = 0;

	if (measure->type != UNKNOWN)
	{
		if (measure->type == LIST)
		{
			PWLAN_AVAILABLE_NETWORK_LIST pwnl = NULL;
			DWORD dwErr = WlanGetAvailableNetworkList(g_hClient, &g_pInterface->InterfaceGuid, NULL, NULL, &pwnl);

			if (ERROR_SUCCESS != dwErr)
			{
				measure->statusString = L"Error";
			}
			else
			{
				// Size of network name can be up to 64 chars, set to 80 to add room for delimiters
				measure->statusString.clear();
				measure->statusString.reserve(80 * measure->listMax);

				UINT printed = 0;  // count of how many networks have been printed already

				// Check all items in WLAN NETWORK LIST
				for (size_t i = 0; i < pwnl->dwNumberOfItems ; ++i)
				{
					if (printed == measure->listMax) break;

					// SSID is in UCHAR, convert to WCHAR
					std::wstring ssid = ConvertToWide((LPCSTR)pwnl->Network[i].dot11Ssid.ucSSID, (int)pwnl->Network[i].dot11Ssid.uSSIDLength);

					// Prevent duplicates that result from profiles, check using SSID
					if (!ssid.empty() && ssid[0] && wcsstr(measure->statusString.c_str(), ssid.c_str()) == NULL)
					{
						++printed;
						measure->statusString += ssid;
						if (measure->listStyle > 0)
						{
							if (measure->listStyle == 1 || measure->listStyle == 3)
							{
								// ADD PHY type
								measure->statusString += L" @";
								measure->statusString += GetDot11Str(pwnl->Network[i].dot11PhyTypes[0], 4);
							}
							if (measure->listStyle == 2 || measure->listStyle == 3)
							{
								// ADD cipher and authentication
								measure->statusString += L" (";
								measure->statusString += GetDot11Str(pwnl->Network[i].dot11DefaultCipherAlgorithm, 1);
								measure->statusString += L':';
								measure->statusString += GetDot11Str(pwnl->Network[i].dot11DefaultAuthAlgorithm, 2);
								measure->statusString += L')';
							}
						}
						measure->statusString += L'\n';
					}
				}

				WlanFreeMemory(pwnl);
			}
		}
		else
		{
			ULONG outsize = 0;
			PWLAN_CONNECTION_ATTRIBUTES wlan_cattr = NULL;
			DWORD dwErr = WlanQueryInterface(g_hClient, &g_pInterface->InterfaceGuid, wlan_intf_opcode_current_connection, NULL, &outsize, (PVOID*)&wlan_cattr, NULL);

			if (ERROR_SUCCESS != dwErr)
			{
				switch (measure->type)
				{
				case SSID:
				case PHY:
				case ENCRYPTION:
				case AUTH:
					measure->statusString = L"-1";
					break;
				}
			}
			else
			{
				switch (measure->type)
				{
				case QUALITY:
					value = (double)wlan_cattr->wlanAssociationAttributes.wlanSignalQuality;
					break;

				case SSID:
					// Need to convert ucSSID to wchar from uchar
					measure->statusString = ConvertToWide((LPCSTR)wlan_cattr->wlanAssociationAttributes.dot11Ssid.ucSSID, (int)wlan_cattr->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
					// If not connected yet add current status
					measure->statusString += GetDot11Str(wlan_cattr->isState, 3);
					break;

				case PHY:
					measure->statusString = GetDot11Str(wlan_cattr->wlanAssociationAttributes.dot11PhyType, 4);
					break;

				case ENCRYPTION:
					measure->statusString = GetDot11Str(wlan_cattr->wlanSecurityAttributes.dot11CipherAlgorithm, 1);
					break;

				case AUTH:
					measure->statusString = GetDot11Str(wlan_cattr->wlanSecurityAttributes.dot11AuthAlgorithm, 2);
					break;

				default:  // Invalid type
					measure->statusString.clear();
					break;
				}

				WlanFreeMemory(wlan_cattr);
			}
		}
	}

	return value;
}


PLUGIN_EXPORT LPCWSTR GetString(void* data)
{
	if (g_pInterface == NULL) return NULL;

	MeasureData* measure = (MeasureData*)data;

	switch (measure->type)
	{
	case LIST:
	case SSID:
	case PHY:
	case ENCRYPTION:
	case AUTH:
		return measure->statusString.c_str();

	default:
		return NULL;
	}
}

PLUGIN_EXPORT void Finalize(void* data)
{
	MeasureData* measure = (MeasureData*)data;
	delete measure;

	if (g_Instances > 0)
	{
		--g_Instances;

		if (g_Instances == 0)
		{
			FinalizeHandle();
		}
	}
}

/*
	switches from winlanapi.h + SDK
	in: -DOT11 ENUM (converted to int)
		-type of ENUM (cipher=1, auth=2, status=3, phy=4, otherwise=error strings)
	out: String to be returned by measure
*/
LPCWSTR GetDot11Str(int dot11enum, int type)
{
	if (type == 1)
	{
		switch (dot11enum)
		{
		case DOT11_CIPHER_ALGO_NONE:
			return L"NONE";
		case DOT11_CIPHER_ALGO_WEP40:
			return L"WEP40";
		case DOT11_CIPHER_ALGO_TKIP:
			return L"TKIP";
		case DOT11_CIPHER_ALGO_CCMP:
			return L"AES";
		case DOT11_CIPHER_ALGO_WEP104:
			return L"WEP104";
		case DOT11_CIPHER_ALGO_WPA_USE_GROUP:
			return L"WPA-GROUP";
		case DOT11_CIPHER_ALGO_WEP:
			return L"WEP";
		default:
			return L"???";
		}
	}
	else if (type == 2)
	{
		switch (dot11enum)
		{
		case DOT11_AUTH_ALGO_80211_OPEN:
			return L"Open";
		case DOT11_AUTH_ALGO_80211_SHARED_KEY:
			return L"Shared";
			case DOT11_AUTH_ALGO_WPA_NONE:
			return L"WPA-NONE";
		case DOT11_AUTH_ALGO_WPA:
			return L"WPA-Enterprise";
		case DOT11_AUTH_ALGO_WPA_PSK:
			return L"WPA-Personal";
		case DOT11_AUTH_ALGO_RSNA:
			return L"WPA2-Enterprise";
		case DOT11_AUTH_ALGO_RSNA_PSK:
			return L"WPA2-Personal";
		default:
			return L"???";
		}
	}
	else if (type == 3)
	{
		switch (dot11enum)
		{
		case wlan_interface_state_connected:
			return L"";
		case wlan_interface_state_authenticating:
			return L"(authorizing...)";
		default:
			return L"(connecting...)";
		}
	}
	else if (type == 4)
	{
		switch (dot11enum)
		{
		case dot11_phy_type_unknown:
		default:
			return L"???";
		case dot11_phy_type_dsss:
			return L"DSSS";
		case dot11_phy_type_erp:
			return L"802.11g";
		case dot11_phy_type_fhss:
			return L"FHSS";
		case dot11_phy_type_hrdsss:
			return L"802.11b";
		case dot11_phy_type_irbaseband:
			return L"IR-Band";
		case dot11_phy_type_ofdm:
			return L"802.11a";
		//Case below appears as dot11_phy_type_ht on MSDN
		//However its not supported in winlanapi.h ???
		case 7:
			return L"802.11n";
		}
	}
	else
	{
		switch (dot11enum)
		{
		case ERROR_INVALID_PARAMETER:
			return L"Invalid parameters";
		case ERROR_NOT_ENOUGH_MEMORY:
			return L"Not enough memory";
		case ERROR_REMOTE_SESSION_LIMIT_EXCEEDED:
			return L"Too many handles already issued";
		default:
			return L"Unknown error code";
		}
	}
}
