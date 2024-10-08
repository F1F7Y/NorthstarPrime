#include "mods/modmanager.h"

//-----------------------------------------------------------------------------
// Check if we're a valve mod
//-----------------------------------------------------------------------------
static bool IsValveMod(const char* pModName)
{
	return (stricmp(pModName, "cstrike") == 0 || stricmp(pModName, "dod") == 0 || stricmp(pModName, "hl1mp") == 0 || stricmp(pModName, "tf") == 0 || stricmp(pModName, "hl2mp") == 0);
}

//-----------------------------------------------------------------------------
// Check if we're a respawn mod
//-----------------------------------------------------------------------------
static bool IsRespawnMod(const char* pModName)
{
	return (stricmp(pModName, "r1") == 0 || stricmp(pModName, "r2") == 0 || stricmp(pModName, "r5") == 0);
}

// ensure that GetLocalBaseClient().m_bRestrictServerCommands is set correctly, which the return value of this function controls
bool (*o_IsValveMod)(const char* pModName);

bool h_IsValveMod(const char* pModName)
{
	return IsValveMod(pModName) || IsRespawnMod(pModName);
}


bool (*o_CEngineAPI__ModInit)(void* self, const char* pModName, const char* pGameDir);

bool h_CEngineAPI__ModInit(void* self, const char* pModName, const char* pGameDir)
{
	if (!o_CEngineAPI__ModInit(self, pModName, pGameDir))
		return false;

	g_pModManager->LoadMods();

	return true;
}

ON_DLL_LOAD("engine.dll", EngineSysDll2, (CModule module))
{
	o_IsValveMod = module.Offset(0x1C6360).RCast<bool (*)(const char*)>();
	HookAttach(&(PVOID&)o_IsValveMod, (PVOID)h_IsValveMod);

	o_CEngineAPI__ModInit = module.Offset(0x1C6670).RCast<bool (*)(void*, const char*, const char*)>();
	HookAttach(&(PVOID&)o_CEngineAPI__ModInit, (PVOID)h_CEngineAPI__ModInit);
}
