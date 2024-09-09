#include <stdio.h>
#include "RestrictedWeapons.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

RestrictedWeapons g_RestrictedWeapons;
PLUGIN_EXPOSE(RestrictedWeapons, g_RestrictedWeapons);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

using namespace DynLibUtils;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayers;

int g_iOnItemPickupId = -1;

std::string g_szBlockSound;
int g_iTypePlayers = 0;
int g_iTypeWeapons = 0;

RWApi* g_pRWApi = nullptr;
IRWApi* g_pRWCore = nullptr;

std::map<std::string, std::string> g_vecPhrases;

std::map<int, std::unordered_map<std::string, int>> g_mRestrictedWeapons;

SH_DECL_MANUALHOOK1(OnItemPickup, 0, 0, 0, bool, CCSWeaponBase *);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void LoadConfigs()
{
	{
		KeyValues* pKV = new KeyValues("RestrictedWeapons");
		char szPath[512];
		g_SMAPI->Format(szPath, sizeof(szPath), "addons/configs/restricted_weapons.ini");
		if (!pKV->LoadFromFile(g_pFullFileSystem, szPath))
		{
			Warning("Failed to load %s\n", szPath);
			return;
		}
		g_mRestrictedWeapons.clear();
		g_iTypePlayers = pKV->GetInt("type_players", 1);
		g_iTypeWeapons = pKV->GetInt("type_weapons", 1);
		g_szBlockSound = strdup(pKV->GetString("block_sound"));
		char szMap[64];
		g_SMAPI->Format(szMap, sizeof(szMap), "%s", g_pUtils->GetCGlobalVars()->mapname);

		KeyValues* pKVDefault = pKV->FindKey("all");
		if(pKVDefault)
		{
			for (KeyValues *pKVPlayer = pKVDefault->GetFirstSubKey(); pKVPlayer; pKVPlayer = pKVPlayer->GetNextKey())
			{
				int iCount = atoi(pKVPlayer->GetName());
				for (KeyValues *pKVSlot = pKVPlayer->GetFirstSubKey(); pKVSlot; pKVSlot = pKVSlot->GetNextKey())
				{
					const char* szWeapon = pKVSlot->GetName();
					int iWeapons = pKVSlot->GetInt(nullptr);
					g_mRestrictedWeapons[iCount][szWeapon] = iWeapons;
				}
			}
		}
		KeyValues* pKVWeapons = pKV->FindKey(szMap);
		if(pKVWeapons)
		{
			for (KeyValues *pKVPlayer = pKVWeapons->GetFirstSubKey(); pKVPlayer; pKVPlayer = pKVPlayer->GetNextKey())
			{
				int iCount = atoi(pKVPlayer->GetName());
				for (KeyValues *pKVSlot = pKVPlayer->GetFirstSubKey(); pKVSlot; pKVSlot = pKVSlot->GetNextKey())
				{
					const char* szWeapon = pKVSlot->GetName();
					int iWeapons = pKVSlot->GetInt(nullptr);
					g_mRestrictedWeapons[iCount][szWeapon] = iWeapons;
				}
			}
		}
	}
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
	LoadConfigs();
}

bool RestrictedWeapons::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	CModule libserver(g_pSource2Server);
	void* pCCSPlayer_WeaponServicesVTable = libserver.GetVirtualTableByName("CCSPlayer_WeaponServices");
	if (!pCCSPlayer_WeaponServicesVTable) return false;
	else
	{
		SH_MANUALHOOK_RECONFIGURE(OnItemPickup, 23, 0, 0);
		g_iOnItemPickupId = SH_ADD_MANUALDVPHOOK(OnItemPickup, pCCSPlayer_WeaponServicesVTable, SH_MEMBER(this, &RestrictedWeapons::Hook_OnItemPickup), false);
	}

	
	g_pRWApi = new RWApi();
	g_pRWCore = g_pRWApi;

	return true;
}

int GetNiggers(int iTeam)
{
	int iCount = 0;
	for(int i = 0; i < 64; i++)
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
		if(!pPlayer) continue;
		CCSPlayerPawn* pPlayerPawn = pPlayer->GetPlayerPawn();
		if(!pPlayerPawn) continue;
		if(g_iTypePlayers == 1) iCount++;
		else if(g_iTypePlayers == 2 && (iTeam == pPlayerPawn->GetTeam())) iCount++;
	}
	return iCount;
}

int GetWeaponCount(const char* szWeapon, int iTeam)
{
	int iCount = 0;
	for(int i = 0; i < 64; i++ )
	{
		CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
		if(!pPlayer) continue;
		CCSPlayerPawn* pPlayerPawn = pPlayer->GetPlayerPawn();
		if(!pPlayerPawn) continue;
		if(g_iTypeWeapons == 2 && iTeam != pPlayerPawn->GetTeam()) continue;
		CCSPlayer_WeaponServices* m_pWeaponServices = pPlayerPawn->m_pWeaponServices();
		if(m_pWeaponServices)
		{
			CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = m_pWeaponServices->m_hMyWeapons();
			
			FOR_EACH_VEC(*weapons, i)
			{
				CBasePlayerWeapon* pWeapon = (*weapons)[i].Get();
				if(!pWeapon) continue;
				if(!strcmp(pWeapon->GetClassname(), szWeapon)) iCount++;
			}
		}
	}
	return iCount;
}

CCSPlayerController* GetPlayer(uint32 iSteamID)
{
    for(int i = 0; i < 64; i++)
    {
        if(g_pPlayers->IsFakeClient(i)) continue;
        if(!g_pPlayers->IsAuthenticated(i)) continue;
        if(!g_pPlayers->IsConnected(i)) continue;
        if(!g_pPlayers->IsInGame(i)) continue;
        int iSteamID2 = g_pPlayers->GetSteamID64(i);
        if(iSteamID2 == iSteamID)
            return CCSPlayerController::FromSlot(i);
    }
    return nullptr;
}

void* RestrictedWeapons::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, RW_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pRWCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

bool RestrictedWeapons::Hook_OnItemPickup(CCSWeaponBase *pWeapon)
{
	if(!g_pUtils) RETURN_META_VALUE(MRES_IGNORED, true);
	CCSPlayer_WeaponServices *pWeaponServices = META_IFACEPTR(CCSPlayer_WeaponServices);
	if(!pWeaponServices) RETURN_META_VALUE(MRES_IGNORED, true);
	CCSPlayerPawn* pPawn = pWeaponServices->__m_pChainEntity();
	if(!pPawn) RETURN_META_VALUE(MRES_IGNORED, true);
	auto pController = (CCSPlayerController*)(pPawn->m_hController().Get());
	if(!pController) RETURN_META_VALUE(MRES_IGNORED, true);
	int iPlayerSlot = pController->GetEntityIndex().Get() - 1;
	if (iPlayerSlot < 0 || iPlayerSlot > 63) RETURN_META_VALUE(MRES_IGNORED, true);
	int iTeam = pPawn->GetTeam();
	int iPlayersCount = GetNiggers(iTeam);

	int iLast = -1;
	int bestFit = -1;

    for (const auto& it : g_mRestrictedWeapons)
    {
		if(iLast == -1 && it.first <= iPlayersCount) {
			iLast = it.first;
			bestFit = iLast;
		}
		else if(it.first > iLast && it.first <= iPlayersCount) {
			iLast = it.first;
			bestFit = iLast;
		}
    }
    if (bestFit != -1)
    {
        const auto& weaponMap = g_mRestrictedWeapons[bestFit];
        const char* weaponClassname = pWeapon->GetClassname();
        auto itWeapon = weaponMap.find(weaponClassname);
        if (itWeapon != weaponMap.end())
        {
            int weaponValue = itWeapon->second;
			int iPlayersWeaponCount = GetWeaponCount(weaponClassname, iTeam);
			if(iPlayersWeaponCount <= weaponValue || weaponValue == -1) RETURN_META_VALUE(MRES_IGNORED, true);
			else {
				if(g_pRWApi->SendOnWeaponRestrictedCallback(iPlayerSlot, weaponClassname)) RETURN_META_VALUE(MRES_IGNORED, true);
				engine->ClientCommand(iPlayerSlot, "play %s", g_szBlockSound.c_str());
				g_pUtils->PrintToChat(iPlayerSlot, g_vecPhrases[g_iTypeWeapons == 2?"block_team":"block"].c_str(), pWeapon->GetWeaponVData()->m_szName().String(), weaponValue);
				CHandle<CCSWeaponBase> hWeapon = pWeapon->GetHandle();
				CHandle<CCSPlayerPawn> hPawn = pPawn->GetHandle();
				g_pUtils->NextFrame([hWeapon, hPawn](){
					CCSWeaponBase *pWeapon = (CCSWeaponBase*)hWeapon.Get();
					if(!pWeapon) return;
					CCSPlayerPawn *pPawn = (CCSPlayerPawn*)hPawn.Get();
					if(!pPawn) return;
					CCSPlayer_WeaponServices *pWeaponServices = pPawn->m_pWeaponServices();
					if(!pWeaponServices) return;
					int iPrice = pWeapon->GetWeaponVData()->m_nPrice();
					uint32 iSteamID = pWeapon->m_OriginalOwnerXuidLow();
					if(iSteamID != 0)
					{
						CCSPlayerController* pPlayer = GetPlayer(iSteamID);
						if(!pPlayer) return;
						CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
						if(!pPawn || !pPawn->IsAlive()) return;
						CCSPlayerController_InGameMoneyServices* pMoneyServices = pPlayer->m_pInGameMoneyServices();
						if(pMoneyServices) {
							pMoneyServices->m_iAccount() += iPrice;
							g_pUtils->SetStateChanged(pPlayer, "CCSPlayerController", "m_pInGameMoneyServices");
						}
					}
					pWeaponServices->DropWeapon(pWeapon);
					g_pUtils->RemoveEntity(pWeapon);
				});
				RETURN_META_VALUE(MRES_SUPERCEDE, false);
			}
        }
    }

	RETURN_META_VALUE(MRES_IGNORED, false);
}

bool RestrictedWeapons::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void RestrictedWeapons::AllPluginsLoaded()
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
	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Players system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pUtils->StartupServer(g_PLID, StartupServer);

	KeyValues::AutoDelete g_kvPhrases("Phrases");
	const char *pszPath = "addons/translations/restricted_weapons.phrases.txt";
	if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}

	std::string szLanguage = std::string(g_pUtils->GetLanguage());
	const char* g_pszLanguage = szLanguage.c_str();
	for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(g_pszLanguage));
}

///////////////////////////////////////
const char* RestrictedWeapons::GetLicense()
{
	return "GPL";
}

const char* RestrictedWeapons::GetVersion()
{
	return "1.0";
}

const char* RestrictedWeapons::GetDate()
{
	return __DATE__;
}

const char *RestrictedWeapons::GetLogTag()
{
	return "RestrictedWeapons";
}

const char* RestrictedWeapons::GetAuthor()
{
	return "Pisex";
}

const char* RestrictedWeapons::GetDescription()
{
	return "Restricted Weapons";
}

const char* RestrictedWeapons::GetName()
{
	return "Restricted Weapons";
}

const char* RestrictedWeapons::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
