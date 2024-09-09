#include <stdio.h>
#include "restricted_weapons_vip.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

restricted_weapons_vip g_restricted_weapons_vip;
PLUGIN_EXPOSE(restricted_weapons_vip, g_restricted_weapons_vip);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IVIPApi* g_pVIPApi;
IRWApi* g_pRWApi;

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

const char* szWeapons[64];

bool restricted_weapons_vip::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	return true;
}

bool restricted_weapons_vip::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

bool OnWeaponRestricted(int iSlot, const char* szWeapon)
{
	if (szWeapons[iSlot][0])
	{
		if(strstr(szWeapons[iSlot], szWeapon)) return true;
	}
	return false;
}

bool OnToggle(int iSlot, const char* szFeature, VIP_ToggleState eOldStatus, VIP_ToggleState& eNewStatus)
{
	if(eNewStatus == ENABLED)
		szWeapons[iSlot] = g_pVIPApi->VIP_GetClientFeatureString(iSlot, "restricted_weapons");
	else
		szWeapons[iSlot] = "";
	return false;
}

void OnClientLoaded(int iSlot, bool bIsVIP)
{
	if(bIsVIP)
		szWeapons[iSlot] = g_pVIPApi->VIP_GetClientFeatureString(iSlot, "restricted_weapons");
	else 
		szWeapons[iSlot] = "";
}

void restricted_weapons_vip::AllPluginsLoaded()
{
	char error[64];
	int ret;
	g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Utils system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pVIPApi = (IVIPApi *)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing VIP system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pRWApi = (IRWApi *)g_SMAPI->MetaFactory(RW_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing RW system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pRWApi->HookOnWeaponRestricted(OnWeaponRestricted);
	g_pVIPApi->VIP_RegisterFeature("restricted_weapons", VIP_BOOL, TOGGLABLE, nullptr, OnToggle);
	g_pVIPApi->VIP_OnClientLoaded(OnClientLoaded);
	g_pUtils->StartupServer(g_PLID, StartupServer);
}

///////////////////////////////////////
const char* restricted_weapons_vip::GetLicense()
{
	return "GPL";
}

const char* restricted_weapons_vip::GetVersion()
{
	return "1.0";
}

const char* restricted_weapons_vip::GetDate()
{
	return __DATE__;
}

const char *restricted_weapons_vip::GetLogTag()
{
	return "restricted_weapons_vip";
}

const char* restricted_weapons_vip::GetAuthor()
{
	return "Pisex";
}

const char* restricted_weapons_vip::GetDescription()
{
	return "restricted_weapons_vip";
}

const char* restricted_weapons_vip::GetName()
{
	return "[Restricted Weapons] VIP";
}

const char* restricted_weapons_vip::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
