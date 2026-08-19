#include "plugin.h"
#include "chat.h"

#include <cstring>

#include <engine/igameeventsystem.h>
#include <interfaces/interfaces.h>
#include <networksystem/inetworkmessages.h>
#include <tier1/convar.h>

SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char *, uint64,
				   const char *, const char *, bool);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, const char *, int, uint64);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char *, uint64);
SH_DECL_HOOK1_void(IServerGameClients, ClientFullyConnect, SH_NOATTRIB, 0, CPlayerSlot);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason,
				   const char *, uint64, const char *);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand &);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext &, const CCommand &);
SH_DECL_HOOK7_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, 0, CCheckTransmitInfo **, int,
	CBitVec<16384> &, CBitVec<16384> &, const Entity2Networkable_t **, const uint16 *, int);
SH_DECL_HOOK3_void(ISource2Server, GameFrame, SH_NOATTRIB, 0, bool, bool, bool);

MMSPlugin g_ThisPlugin;
BrightnessController g_Brightness;

IVEngineServer *g_pEngine = nullptr;
ICvar *g_pICvar = nullptr;
IServerGameClients *g_pGameClients = nullptr;
IGameEventSystem *g_pGameEventSystem = nullptr;

PLUGIN_EXPOSE(MMSPlugin, g_ThisPlugin);

namespace
{
bool IsBrightnessCommand(const char *command)
{
	return command &&
		(V_stricmp(command, "drop") == 0 || V_stricmp(command, "cs2visuals_cycle") == 0);
}

bool HandleBrightnessCommand(int playerSlot, const CCommand &args)
{
	if (playerSlot < 0 || !IsBrightnessCommand(args.Arg(0)))
	{
		return false;
	}

	g_Brightness.Cycle(playerSlot);
	return true;
}

void BindBrightnessKey(CPlayerSlot slot)
{
	if (g_pEngine && slot.Get() >= 0)
	{
		// ClientCommand is formatted by the engine and sent to that client. The
		// explicit command avoids relying on KZ's server-side drop handling.
		g_pEngine->ClientCommand(slot, "bind g \"cs2visuals_cycle\"");
	}
}
} // namespace

CON_COMMAND_F(cs2visuals_cycle, "Cycle the CS2 Visuals brightness level",
			  FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE)
{
	const CPlayerSlot player = context.GetPlayerSlot();
	if (HandleBrightnessCommand(player.Get(), args))
	{
		return;
	}
}

bool MMSPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_ANY(GetEngineFactory, g_pEngine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_ANY(GetEngineFactory, g_pICvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pGameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
	GET_V_IFACE_ANY(GetEngineFactory, g_pNetworkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pGameEventSystem, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService,
		GAMERESOURCESERVICESERVER_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameEntities, ISource2GameEntities,
		SOURCE2GAMEENTITIES_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	if (!g_PostProcess.Initialize(error, maxlen))
	{
		META_CONPRINTF("[CS2 Visuals] Post-processing initialization failed: %s\n", error);
		return false;
	}

	g_pCVar = g_pICvar;
	g_SMAPI->AddListener(this, this);

	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientFullyConnect, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientFullyConnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pICvar,
				SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);
	SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities,
				SH_MEMBER(this, &MMSPlugin::Hook_CheckTransmit), true);
	SH_ADD_HOOK(ISource2Server, GameFrame, g_pSource2Server,
				SH_MEMBER(this, &MMSPlugin::Hook_GameFrame), true);

	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);
	META_CONPRINTF("[CS2 Visuals] Loaded v%s; Source 2 post-processing active. G will cycle brightness levels.\n", GetVersion());
	return true;
}

bool MMSPlugin::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK(IServerGameClients, OnClientConnected, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_REMOVE_HOOK(IServerGameClients, ClientPutInServer, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientActive, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientFullyConnect, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientFullyConnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pICvar,
				SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);
	SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities,
				SH_MEMBER(this, &MMSPlugin::Hook_CheckTransmit), true);
	SH_REMOVE_HOOK(ISource2Server, GameFrame, g_pSource2Server,
				SH_MEMBER(this, &MMSPlugin::Hook_GameFrame), true);

	g_PostProcess.Shutdown();
	return true;
}

void MMSPlugin::AllPluginsLoaded()
{
	// The file is shipped in cfg/cs2visuals and is loaded after plugin cvars exist.
	if (g_pEngine)
	{
		g_pEngine->ServerCommand("exec cs2visuals/cs2visuals.cfg\n");
	}
}

void MMSPlugin::OnLevelInit(char const *mapName, char const *mapEntities, char const *oldLevel,
							char const *landmarkName, bool loadGame, bool background)
{
	(void)mapName;
	(void)mapEntities;
	(void)oldLevel;
	(void)landmarkName;
	(void)loadGame;
	(void)background;
	g_Brightness.ResetAll();
}

void MMSPlugin::OnLevelShutdown()
{
	g_PostProcess.ClearForMapChange();
	g_Brightness.ResetAll();
}

void MMSPlugin::Hook_OnClientConnected(CPlayerSlot slot, const char *name, uint64 xuid,
									   const char *networkId, const char *address, bool fakePlayer)
{
	(void)name;
	(void)xuid;
	(void)networkId;
	(void)address;
	(void)fakePlayer;
	g_Brightness.OnClientConnected(slot.Get());
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientPutInServer(CPlayerSlot slot, const char *name, int type, uint64 xuid)
{
	(void)name;
	(void)type;
	(void)xuid;
	g_Brightness.OnClientPutInServer(slot.Get());
	BindBrightnessKey(slot);
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientActive(CPlayerSlot slot, bool loadGame, const char *name, uint64 xuid)
{
	(void)loadGame;
	(void)name;
	(void)xuid;
	// KZ servers do not need the original drop action. Bind G to the explicit
	// plugin command so the feature works even when drop is handled internally.
	BindBrightnessKey(slot);
	CS2VisualsChat(slot.Get(), "[CS2 Visuals] Loaded. Press G to cycle brightness: default -> 1x -> 2x -> 3x.");
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientFullyConnect(CPlayerSlot slot)
{
	// Some clients reject server-issued binds until the fully-connected stage.
	// Repeat the bind here so both the normal and KZ connection paths work.
	BindBrightnessKey(slot);
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *name,
										uint64 xuid, const char *networkId)
{
	(void)reason;
	(void)name;
	(void)xuid;
	(void)networkId;
	g_Brightness.OnClientDisconnect(slot.Get());
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientCommand(CPlayerSlot slot, const CCommand &args)
{
	if (HandleBrightnessCommand(slot.Get(), args))
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext &context, const CCommand &args)
{
	(void)cmd;
	const CPlayerSlot player = context.GetPlayerSlot();
	if (HandleBrightnessCommand(player.Get(), args))
	{
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_CheckTransmit(CCheckTransmitInfo **infos, int count, CBitVec<16384> &unionTransmitEdicts,
	CBitVec<16384> &unknown, const Entity2Networkable_t **networkables, const uint16 *entityIndices, int entityCount)
{
	(void)unionTransmitEdicts;
	(void)unknown;
	(void)networkables;
	(void)entityIndices;
	(void)entityCount;
	g_PostProcess.FilterTransmit(infos, count);
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_GameFrame(bool simulating, bool firstTick, bool lastTick)
{
	(void)simulating;
	(void)firstTick;
	(void)lastTick;
	g_PostProcess.ProcessPending();
	RETURN_META(MRES_IGNORED);
}
