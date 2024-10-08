#include "game/client/hud_chat.h"

#include "game/server/gameinterface.h"
#include "vscript/vscript.h"

void (*o_CHudChat__AddGameLine)(void* self, const char* message, int inboxId, bool isTeam, bool isDead);

// clang-format off
void h_CHudChat__AddGameLine(void* self, const char* message, int inboxId, bool isTeam, bool isDead)
// clang-format on
{
	// This hook is called for each HUD, but we only want our logic to run once.
	if (self != *CHudChat::allHuds)
		return;

	int senderId = inboxId & CUSTOM_MESSAGE_INDEX_MASK;
	bool isAnonymous = senderId == 0;
	bool isCustom = isAnonymous || (inboxId & CUSTOM_MESSAGE_INDEX_BIT);

	// Type is set to 0 for non-custom messages, custom messages have a type encoded as the first byte
	int type = 0;
	const char* payload = message;
	if (isCustom)
	{
		type = message[0];
		payload = message + 1;
	}

	RemoveAsciiControlSequences(const_cast<char*>(message), true);

	int nRetValue = SQRESULT_ERROR;
	if (g_pClientVM && g_pClientVM->GetVM())
	{
		ScriptContext nContext = (ScriptContext)g_pClientVM->vmContext;
		HSQUIRRELVM hVM = g_pClientVM->GetVM();
		const char* pszFuncName = "CHudChat_ProcessMessageStartThread";

		SQObject oFunction {};
		int nResult = sq_getfunction(hVM, pszFuncName, &oFunction, 0);
		if (nResult == 0)
		{
			sq_pushobject(hVM, &oFunction);
			sq_pushroottable(hVM);

			sq_pushinteger(hVM, static_cast<int>(senderId) - 1);
			sq_pushstring(hVM, payload, -1);
			sq_pushbool(hVM, isTeam);
			sq_pushbool(hVM, isDead);
			sq_pushinteger(hVM, type);

			nRetValue = sq_call(hVM, 6, false, false);
		}
		else
		{
			Error(VScript_GetNativeLogContext(nContext), NO_ERROR, "Call was unable to find function with name '%s'. Is it global?\n", pszFuncName);
		}
	}

	if (nRetValue == SQRESULT_ERROR)
		for (CHudChat* hud = *CHudChat::allHuds; hud != NULL; hud = hud->next)
			o_CHudChat__AddGameLine(hud, message, inboxId, isTeam, isDead);
}

ON_DLL_LOAD_CLIENT("client.dll", HudChat, (CModule module))
{
	o_CHudChat__AddGameLine = module.Offset(0x22E580).RCast<void (*)(void*, const char*, int, bool, bool)>();
	HookAttach(&(PVOID&)o_CHudChat__AddGameLine, (PVOID)h_CHudChat__AddGameLine);
}
