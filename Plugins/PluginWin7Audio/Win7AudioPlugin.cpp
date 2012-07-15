/*
  Copyright (C) 2010 Stefan Hiller

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

#include <windows.h>
#include <string>
#include <vector>
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "PolicyConfig.h"
#include "../API/RainmeterAPI.h"
#include "../../Library/DisableThreadLibraryCalls.h"	// contains DllMain entry point

#define SAFE_RELEASE(punk)  \
			  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }

static BOOL com_initialized = FALSE;
static BOOL instance_created = FALSE;
static BOOL is_mute = FALSE;
static float master_volume = 0.5f;

static enum VolumeAction
{
	INIT,
	TOGGLE_MUTE,
	GET_VOLUME
};

const static CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const static IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const static IID IID_CPolicyConfigClient = __uuidof(CPolicyConfigClient);
const static IID IID_IPolicyConfig = __uuidof(IPolicyConfig);
const static IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

static IMMDeviceEnumerator *pEnumerator = 0;
static IMMDeviceCollection *pCollection = 0;

static std::vector<std::wstring> endpointIDs;

UINT CleanUp()
{
	SAFE_RELEASE(pCollection);
	SAFE_RELEASE(pEnumerator);
	instance_created = false;
	return 0;
}

bool InitCom()
{
	if (!com_initialized) com_initialized = SUCCEEDED(CoInitialize(0));
	if (!com_initialized)
	{
		RmLog(LOG_ERROR, L"Win7AudioPlugin.dll: COM initialization failed");
		return false;
	}
	HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL,
								  IID_IMMDeviceEnumerator, (void**)&pEnumerator);
	instance_created = (S_OK == hr && pEnumerator);
	if (!instance_created)
	{
		std::wstring dbg_str = L"Win7AudioPlugin.dll: COM creation failed";
		if (hr == REGDB_E_CLASSNOTREG) dbg_str += L" REGDB_E_CLASSNOTREG";
		else if (hr == CLASS_E_NOAGGREGATION) dbg_str += L" CLASS_E_NOAGGREGATION";
		else if (hr == E_NOINTERFACE) dbg_str += L" E_NOINTERFACE";
		else
		{
			static WCHAR e_code[256];
			wsprintf(e_code, L" %li", (long)hr);

			dbg_str += e_code;
		}
		RmLog(LOG_ERROR, dbg_str.c_str());
		return CleanUp() != 0;
	}
	if (S_OK != pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection) || !pCollection)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Could not enumerate AudioEndpoints");
		return CleanUp() != 0;
	}
	return true;
}

void UnInitCom()
{
	CleanUp();
	if (com_initialized) CoUninitialize();
	com_initialized = FALSE;
}

HRESULT RegisterDevice(PCWSTR devID)
{
	HRESULT hr = S_FALSE;
	try
	{
		InitCom();
		IPolicyConfig *pPolicyConfig;

		hr = CoCreateInstance(IID_CPolicyConfigClient, NULL,
							CLSCTX_ALL, IID_IPolicyConfig,
							(LPVOID *)&pPolicyConfig);
		if (hr == S_OK)
		{
			hr = pPolicyConfig->SetDefaultEndpoint(devID, eConsole);
			if (hr == S_OK)
			{
				hr = pPolicyConfig->SetDefaultEndpoint(devID, eCommunications);
			}
			SAFE_RELEASE(pPolicyConfig);
		}
	}
	catch (...)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: RegisterDevice exception");
		hr = S_FALSE;
	}
	UnInitCom();
	return hr;
}

std::wstring GetDefaultID()
{
	std::wstring id_default;
	IMMDevice * pEndpoint = 0;
	try
	{
		if (pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint) == S_OK)
		{
			LPWSTR pwszID = 0;
			if (pEndpoint->GetId(&pwszID) == S_OK)
			{
				id_default = pwszID;
			}
			CoTaskMemFree(pwszID);
		}
	}
	catch (...)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: GetDefaultID exception");
		id_default = L"Exception";
	}
	SAFE_RELEASE(pEndpoint)
	return id_default;
}

bool GetWin7AudioState(const VolumeAction action)
{
	IMMDevice * pEndpoint = 0;
	IAudioEndpointVolume * pEndptVol = 0;
	bool success = false;

	try
	{
		if (InitCom())
		{
			if (pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint) == S_OK)
			{
				if (pEndpoint->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, 0, (void**)&pEndptVol) == S_OK)
				{
					if (pEndptVol->GetMute(&is_mute) == S_OK && action == TOGGLE_MUTE)
					{
						success = pEndptVol->SetMute(is_mute == TRUE ? FALSE : TRUE, 0) == S_OK;
					}
					// get current volume
					float vol = 0.0f;
					if (action != TOGGLE_MUTE && pEndptVol->GetMasterVolumeLevelScalar(&vol) == S_OK)
					{
						master_volume = vol;
					}
				}
			}
		}
	}
	catch (...)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: ToggleMute exception");
	}
	SAFE_RELEASE(pEndptVol)
	SAFE_RELEASE(pEndpoint)
	UnInitCom();
	return success;
}

UINT GetIndex()
{
	std::wstring id_default;
	if (InitCom()) id_default = GetDefaultID();
	UnInitCom();

	for (UINT i = 0; i < endpointIDs.size(); ++i)
	{
		if (_wcsicmp(endpointIDs[i].c_str(), id_default.c_str()) == 0) return i + 1;
	}
	return 0;
}

bool SetWin7Volume(UINT volume, int offset = 0)
{
	IMMDevice * pEndpoint = 0;
	IAudioEndpointVolume * pEndptVol = 0;
	bool success = false;

	try
	{
		if (InitCom())
		{
			if (pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint) == S_OK)
			{
				if (pEndpoint->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, 0, (void**)&pEndptVol) == S_OK)
				{
					pEndptVol->SetMute(FALSE, 0);
					float vol = 0.0f;
					if (offset != 0) // change master volume + offset
					{
						float off = static_cast<float>(offset) / 100.0f;
						vol = master_volume + off;
						vol = (vol < 0.0f) ? 0.0f : ((vol > 1.0f) ? 1.0f : vol);
					}
					else
					{
						vol = (float)volume / 100.0f;
					}
					// set to volume
					success = pEndptVol->SetMasterVolumeLevelScalar(vol, 0) == S_OK;
					if (success) success = pEndptVol->GetMasterVolumeLevelScalar(&vol) == S_OK;
					if (success) master_volume = vol;
				}
			}
		}

	}
	catch (...)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: SetVolume exception");
	}
	SAFE_RELEASE(pEndptVol)
	SAFE_RELEASE(pEndpoint)
	UnInitCom();
	return success;
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	if (!InitCom())
	{
		UnInitCom();
		return;
	}

	UINT count;
	if (!pCollection || (S_OK != pCollection->GetCount(&count)))
	{
		UnInitCom();
		return;
	}
	endpointIDs = std::vector<std::wstring>(count);

	for (UINT i = 0; i < count; ++i)
	{
		IMMDevice *pEndpoint = 0;

		// Get pointer to endpoint number i.
		if (pCollection->Item(i, &pEndpoint) == S_OK)
		{
			// Get the endpoint ID string.
			LPWSTR pwszID = 0;
			if (pEndpoint->GetId(&pwszID) == S_OK)
			{
				endpointIDs[i] = pwszID;
			}
			CoTaskMemFree(pwszID);
		}
		SAFE_RELEASE(pEndpoint)
	}
	UnInitCom();
	GetWin7AudioState(INIT);
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	*maxValue = 100;
}

PLUGIN_EXPORT double Update(void* data)
{
	GetWin7AudioState(GET_VOLUME);
	double volume = is_mute == TRUE ? -1.0 : floor(master_volume * 100.0 + 0.5);	// rounding up at 0.5
	return volume > 100.0 ? 100.0 : volume;
}

PLUGIN_EXPORT LPCWSTR GetString(void* data)
{
	static WCHAR result[256];
	wsprintf(result, L"ERROR");
	try {
		if (!InitCom() || !pEnumerator)
		{
			UnInitCom();
			wsprintf(result, L"ERROR - Initializing COM");
			return result;
		}

		IMMDevice * pEndpoint = 0;
		if (pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint) == S_OK)
		{
			IPropertyStore * pProps = 0;
			if (pEndpoint->OpenPropertyStore(STGM_READ, &pProps) == S_OK)
			{
				PROPVARIANT varName;
				PropVariantInit(&varName);
				if (pProps->GetValue(PKEY_Device_DeviceDesc, &varName) == S_OK)
				{
					wcsncpy(result, varName.pwszVal, 255);
					PropVariantClear(&varName);
					SAFE_RELEASE(pProps)
					SAFE_RELEASE(pEndpoint)
					UnInitCom();
					return result;
				}
				else
				{
					PropVariantClear(&varName);
					SAFE_RELEASE(pProps)
					SAFE_RELEASE(pEndpoint)
					wsprintf(result, L"ERROR - Getting Device Description");
				}
			}
			else
			{
				SAFE_RELEASE(pProps)
				SAFE_RELEASE(pEndpoint)
				wsprintf(result, L"ERROR - Getting Property");
			}
		}
		else
		{
			SAFE_RELEASE(pEndpoint)
			wsprintf(result, L"ERROR - Getting Default Device");
		}
	} catch (...)
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: GetString exception");
		wsprintf(result, L"Exception");
	}
	UnInitCom();
	return result;
}

PLUGIN_EXPORT void Finalize(void* data)
{
	UnInitCom();
}

PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args)
{
	std::wstring wholeBang = args;

	size_t pos = wholeBang.find(' ');
	if (pos != -1)
	{
		std::wstring bang = wholeBang.substr(0, pos);
		wholeBang.erase(0, pos + 1);

		if (_wcsicmp(bang.c_str(), L"SetOutputIndex") == 0)
		{
			// Parse parameters
			int index = 0;
			if (1 == swscanf_s(wholeBang.c_str(), L"%d", &index))
			{
				if (endpointIDs.size() <= 0)
				{
					RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: No device found");
					return;
				}
				// set to endpoint [index-1]
				if (index <= 0) index = 1;
				else if (index > (int)endpointIDs.size()) index = (int)endpointIDs.size();
				RegisterDevice(endpointIDs[index - 1].c_str());
			}
			else
			{
				RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Incorrect number of arguments for bang");
			}
		}
		else if (_wcsicmp(bang.c_str(), L"SetVolume") == 0)
		{
			// Parse parameters
			int volume = 0;
			if (1 == swscanf_s(wholeBang.c_str(), L"%d", &volume))
			{
				if (!SetWin7Volume(volume < 0 ? 0 : (volume > 100 ? 100 : (UINT)volume)))
				{
					RmLog(LOG_ERROR, L"Win7AudioPlugin.dll: Error setting volume");
				}
			}
			else
			{
				RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Incorrect number of arguments for bang");
			}
		}
		else if (_wcsicmp(bang.c_str(), L"ChangeVolume") == 0)
		{
			// Parse parameters
			int offset = 0;
			if (1 == swscanf_s(wholeBang.c_str(), L"%d", &offset) && offset)
			{
				if (!SetWin7Volume(0, offset))
				{
					RmLog(LOG_ERROR, L"Win7AudioPlugin.dll: Error changing volume");
				}
			}
			else
			{
				RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Incorrect number of arguments for bang");
			}
		}
		else
		{
			RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Unknown bang");
		}

	}
	else if (_wcsicmp(wholeBang.c_str(), L"ToggleNext") == 0)
	{
		//RmLog(LOG_NOTICE, L"Win7AudioPlugin.dll: Next device.");
		const UINT i = GetIndex();
		if (i) RegisterDevice(endpointIDs[(i == endpointIDs.size()) ? 0 : i].c_str());
		else RmLog(LOG_ERROR, L"Win7AudioPlugin.dll: Update error");
	}
	else if (_wcsicmp(wholeBang.c_str(), L"TogglePrevious") == 0)
	{
		const UINT i = GetIndex();
		if (i) RegisterDevice(endpointIDs[(i == 1) ? endpointIDs.size() - 1 : i - 2].c_str());
		else RmLog(LOG_ERROR, L"Win7AudioPlugin.dll: Update error");
	}
	else if (_wcsicmp(wholeBang.c_str(), L"ToggleMute") == 0)
	{
		GetWin7AudioState(TOGGLE_MUTE);
	}
	else if (_wcsicmp(wholeBang.c_str(), L"Mute") == 0)
	{
		if (!is_mute)
		{
			GetWin7AudioState(TOGGLE_MUTE);
		}
	}
	else if (_wcsicmp(wholeBang.c_str(), L"Unmute") == 0)
	{
		if (is_mute)
		{
			GetWin7AudioState(TOGGLE_MUTE);
		}
	}
	else
	{
		RmLog(LOG_WARNING, L"Win7AudioPlugin.dll: Unknown bang");
	}
}