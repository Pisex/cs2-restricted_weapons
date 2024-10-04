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
int g_iUnblockType = 0;

RWApi* g_pRWApi = nullptr;
IRWApi* g_pRWCore = nullptr;

std::map<std::string, std::string> g_vecPhrases;

std::map<int, std::unordered_map<std::string, int>> g_mRestrictedWeapons;

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

const char* GetWeaponByDefIndex(int iIndex)
{
	const char* szWeapon = nullptr;
	switch(iIndex)
	{
		case 1: szWeapon = "weapon_deagle"; break;
		case 2: szWeapon = "weapon_elite"; break;
		case 3: szWeapon = "weapon_fiveseven"; break;
		case 4: szWeapon = "weapon_glock"; break;
		case 7: szWeapon = "weapon_ak47"; break;
		case 8: szWeapon = "weapon_aug"; break;
		case 9: szWeapon = "weapon_awp"; break;
		case 10: szWeapon = "weapon_famas"; break;
		case 11: szWeapon = "weapon_g3sg1"; break;
		case 13: szWeapon = "weapon_galilar"; break;
		case 14: szWeapon = "weapon_m249"; break;
		case 16: szWeapon = "weapon_m4a1"; break;
		case 17: szWeapon = "weapon_mac10"; break;
		case 19: szWeapon = "weapon_p90"; break;
		case 23: szWeapon = "weapon_mp5sd"; break;
		case 24: szWeapon = "weapon_ump45"; break;
		case 25: szWeapon = "weapon_xm1014"; break;
		case 26: szWeapon = "weapon_bizon"; break;
		case 27: szWeapon = "weapon_mag7"; break;
		case 28: szWeapon = "weapon_negev"; break;
		case 29: szWeapon = "weapon_sawedoff"; break;
		case 30: szWeapon = "weapon_tec9"; break;
		case 31: szWeapon = "weapon_taser"; break;
		case 32: szWeapon = "weapon_hkp2000"; break;
		case 33: szWeapon = "weapon_mp7"; break;
		case 34: szWeapon = "weapon_mp9"; break;
		case 35: szWeapon = "weapon_nova"; break;
		case 36: szWeapon = "weapon_p250"; break;
		case 38: szWeapon = "weapon_scar20"; break;
		case 39: szWeapon = "weapon_sg556"; break;
		case 40: szWeapon = "weapon_ssg08"; break;
		case 60: szWeapon = "weapon_m4a1_silencer"; break;
		case 61: szWeapon = "weapon_usp_silencer"; break;
		case 63: szWeapon = "weapon_cz75a"; break;
		case 64: szWeapon = "weapon_revolver"; break;
		default: szWeapon = "weapon_knife"; break;
	}
	return szWeapon;
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
		g_iUnblockType = pKV->GetInt("unblock_type", 0);
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

bool RestrictedWeapons::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void OnItemPickup(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	int iSlot = pEvent->GetInt("userid");
	if(iSlot < 0 || iSlot > 64) return;
	CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(iSlot);
	if(!pPlayer) return;
	CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
	if(!pPawn) return;
	CCSPlayer_WeaponServices* m_pWeaponServices = pPawn->m_pWeaponServices();
	if(!m_pWeaponServices) return;
	int iDefIndex = pEvent->GetInt("defindex");
	CUtlVector<CHandle<CBasePlayerWeapon>>* weapons = m_pWeaponServices->m_hMyWeapons();
	CCSWeaponBase* pWeapon = nullptr;
	for(int i = 0; i < weapons->Count(); i++)
	{
		CCSWeaponBase* pWeapon2 = (CCSWeaponBase*)weapons->Element(i).Get();
		if(!pWeapon2) continue;
		if(pWeapon2->m_AttributeManager().m_Item().m_iItemDefinitionIndex() == iDefIndex) {
			pWeapon = pWeapon2;
			break;
		}
	}
	if(!pWeapon) return;
	const char* szWeapon = GetWeaponByDefIndex(iDefIndex);
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
        auto itWeapon = weaponMap.find(szWeapon);
        if (itWeapon != weaponMap.end())
        {
            int weaponValue = itWeapon->second;
			int iPlayersWeaponCount = GetWeaponCount(szWeapon, iTeam);
			if(iPlayersWeaponCount <= weaponValue || weaponValue == -1) return;
			else {
				if(g_pRWApi->SendOnWeaponRestrictedCallback(iSlot, szWeapon))
				{
					if(g_iUnblockType == 1 && weaponValue > 0) return;
					else if(g_iUnblockType == 0) return;
				}
				engine->ClientCommand(iSlot, "play %s", g_szBlockSound.c_str());
				g_pUtils->PrintToChat(iSlot, g_vecPhrases[g_iTypeWeapons == 2?"block_team":"block"].c_str(), g_vecPhrases[szWeapon].c_str(), weaponValue);
				CHandle<CCSWeaponBase> hWeapon = pWeapon->GetHandle();
				CHandle<CCSPlayerPawn> hPawn = pPawn->GetHandle();
				g_pUtils->NextFrame([iSlot, hWeapon, hPawn](){
					CCSWeaponBase *pWeapon = (CCSWeaponBase*)hWeapon.Get();
					if(!pWeapon) return;
					CCSPlayerPawn *pPawn = (CCSPlayerPawn*)hPawn.Get();
					if(!pPawn) return;
					int iPrice = pWeapon->GetWeaponVData()->m_nPrice();
					uint32 iSteamID = pWeapon->m_OriginalOwnerXuidLow();
					if(iSteamID != 0)
					{
						CCSPlayerController* pPlayer = GetPlayer(iSteamID);
						if(pPlayer)
						{
							CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
							if(pPawn && pPawn->IsAlive())
							{
								CCSPlayerController_InGameMoneyServices* pMoneyServices = pPlayer->m_pInGameMoneyServices();
								if(pMoneyServices) {
									pMoneyServices->m_iAccount() += iPrice;
									g_pUtils->SetStateChanged(pPlayer, "CCSPlayerController", "m_pInGameMoneyServices");
								}
							}
						}
					}
					g_pPlayers->DropWeapon(iSlot, pWeapon);
					g_pUtils->RemoveEntity(pWeapon);
				});
			}
        }
    }

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

	g_pUtils->HookEvent(g_PLID, "item_pickup", OnItemPickup);
	g_pUtils->HookEvent(g_PLID, "item_equip", OnItemPickup);
}

///////////////////////////////////////
const char* RestrictedWeapons::GetLicense()
{
	return "GPL";
}

const char* RestrictedWeapons::GetVersion()
{
	return "1.1";
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
