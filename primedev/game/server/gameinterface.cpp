#include "game/server/gameinterface.h"
#include "game/server/enginecallback.h"
#include "game/server/recipientfilter.h"

#include "vscript/vscript.h"
#include "shared/exploit_fixes/ns_limits.h"
#include "engine/server/server.h"
#include "game/server/util_server.h"

void (*o_CServerGameDLL__OnReceivedSayTextMessage)(CServerGameDLL* self, unsigned int senderPlayerId, const char* text, bool isTeam);

void h_CServerGameDLL__OnReceivedSayTextMessage(CServerGameDLL* self, unsigned int senderPlayerId, const char* text, bool isTeam)
{
	// We first go through script then call the original from Script_NSSendMessage
	// This allows script to modify the message
	RemoveAsciiControlSequences(const_cast<char*>(text), true);

	// check chat ratelimits
	if (!g_pServerLimits->CheckChatLimits(g_pServer->GetClient(senderPlayerId - 1)))
		return;

	int nRetValue = SQRESULT_ERROR;
	if (g_pServerVM && g_pServerVM->GetVM())
	{
		ScriptContext nContext = (ScriptContext)g_pServerVM->vmContext;
		HSQUIRRELVM hVM = g_pServerVM->GetVM();
		const char* pszFuncName = "CServerGameDLL_ProcessMessageStartThread";

		SQObject oFunction {};
		int nResult = sq_getfunction(hVM, pszFuncName, &oFunction, 0);
		if (nResult == 0)
		{
			sq_pushobject(hVM, &oFunction);
			sq_pushroottable(hVM);

			sq_pushinteger(hVM, static_cast<int>(senderPlayerId) - 1);
			sq_pushstring(hVM, text, -1);
			sq_pushbool(hVM, isTeam);

			nRetValue = sq_call(hVM, 4, false, false);
		}
		else
		{
			Error(VScript_GetNativeLogContext(nContext), NO_ERROR, "Call was unable to find function with name '%s'. Is it global?\n", pszFuncName);
		}
	}

	if (nRetValue == SQRESULT_ERROR)
		o_CServerGameDLL__OnReceivedSayTextMessage(self, senderPlayerId, text, isTeam);
}

void Chat_SendMessage(unsigned int playerIndex, const char* text, bool isTeam)
{
	RemoveAsciiControlSequences(const_cast<char*>(text), true);
	o_CServerGameDLL__OnReceivedSayTextMessage(g_pServerGameDLL,
											   // Ensure the first bit isn't set, since this indicates a custom message
											   (playerIndex + 1) & CUSTOM_MESSAGE_INDEX_MASK, text, isTeam);
}

void Chat_BroadcastMessage(int fromPlayerIndex, int toPlayerIndex, const char* text, bool isTeam, bool isDead, CustomMessageType messageType)
{
	CPlayer* toPlayer = NULL;
	if (toPlayerIndex >= 0)
	{
		toPlayer = UTIL_PlayerByIndex(toPlayerIndex + 1);
		if (toPlayer == NULL)
			return;
	}

	// Build a new string where the first byte is the message type
	char sendText[256];
	sendText[0] = (char)messageType;
	strncpy_s(sendText + 1, 255, text, 254);

	// Anonymous custom messages use playerId=0, non-anonymous ones use a player ID with the first bit set
	unsigned int fromPlayerId = fromPlayerIndex < 0 ? 0 : ((fromPlayerIndex + 1) | CUSTOM_MESSAGE_INDEX_BIT);

	CRecipientFilter filter;
	CRecipientFilter__Construct(&filter);
	if (toPlayer == NULL)
	{
		CRecipientFilter__AddAllPlayers(&filter);
	}
	else
	{
		CRecipientFilter__AddRecipient(&filter, toPlayer);
	}
	CRecipientFilter__MakeReliable(&filter);

	UserMessageBegin(&filter, "SayText");
	MessageWriteByte(fromPlayerId);
	MessageWriteString(sendText);
	MessageWriteBool(isTeam);
	MessageWriteBool(isDead);
	MessageEnd();

	CRecipientFilter__Destruct(&filter);
}

ON_DLL_LOAD("server.dll", GameInterface, (CModule module))
{
	o_CServerGameDLL__OnReceivedSayTextMessage = module.Offset(0x1595C0).RCast<void (*)(CServerGameDLL*, unsigned int, const char*, bool)>();
	HookAttach(&(PVOID&)o_CServerGameDLL__OnReceivedSayTextMessage, (PVOID)h_CServerGameDLL__OnReceivedSayTextMessage);

	g_pServerGameClients = Sys_GetFactoryPtr("server.dll", "ServerGameClients004").RCast<CServerGameClients*>();
	g_pServerGameDLL = Sys_GetFactoryPtr("server.dll", "ServerGameDLL005").RCast<CServerGameDLL*>();

	CServerGameDLL__OnReceivedSayTextMessage = module.Offset(0x1595C0).RCast<void(__fastcall*)(CServerGameDLL*, unsigned int, const char*, int)>();

	UserMessageBegin = module.Offset(0x15C520).RCast<void(__fastcall*)(CRecipientFilter*, const char*)>();
	MessageEnd = module.Offset(0x158880).RCast<void(__fastcall*)()>();
	MessageWriteByte = module.Offset(0x158A90).RCast<void(__fastcall*)(int)>();
	MessageWriteString = module.Offset(0x158D00).RCast<void(__fastcall*)(const char*)>();
	MessageWriteBool = module.Offset(0x158A00).RCast<void(__fastcall*)(bool)>();

	// nop out call to VGUI shutdown  in CServerGameDLL::DLLShutdown
	// since it crashes the game when quitting from the console
	module.Offset(0x154A96).NOP(5);
}
