#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include <utlstring.h>
#include <KeyValues.h>
#include "CCSPlayerController.h"
#include "include/menus.h"
#include "include/restricted_weapons.h"
#include "module.h"

class RestrictedWeapons final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
private:
	bool Hook_OnItemPickup(CCSWeaponBase *pWeapon);
};

class RWApi : public IRWApi
{
private:
	std::vector<OnWeaponRestrictedCallback> m_OnWeaponRestrictedCallbacks;
public:
	void HookOnWeaponRestricted(OnWeaponRestrictedCallback callback) override {
		m_OnWeaponRestrictedCallbacks.push_back(callback);
	}
	
	bool SendOnWeaponRestrictedCallback(int iSlot, const char* szWeapon) {
		bool bFound = false;
		for(auto& item : m_OnWeaponRestrictedCallbacks)
		{
			if (item && item(iSlot, szWeapon)) {
				bFound = true;
			}
		}
		return bFound;
	}
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
