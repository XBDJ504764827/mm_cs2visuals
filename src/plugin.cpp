#include "plugin.h"
#include "chat.h"

#include <cstring>

#include <engine/igameeventsystem.h>
#include <networksystem/inetworkmessages.h>
#include <tier1/convar.h>

SH_DECL_HOOK6_void(IServerGameClients, OnClientConnected, SH_NOATTRIB, 0, CPlayerSlot, const char *, uint64,
				   const char *, const char *, bool);
SH_DECL_HOOK4_void(IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, const char *, int, uint64);
SH_DECL_HOOK4_void(IServerGameClients, ClientActive, SH_NOATTRIB, 0, CPlayerSlot, bool, const char *, uint64);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason,
				   const char *, uint64, const char *);
SH_DECL_HOOK2_void(IServerGameClients, ClientCommand, SH_NOATTRIB, 0, CPlayerSlot, const CCommand &);
SH_DECL_HOOK3_void(ICvar, DispatchConCommand, SH_NOATTRIB, 0, ConCommandRef, const CCommandContext &, const CCommand &);

MMSPlugin g_ThisPlugin;
BrightnessController g_Brightness;

IVEngineServer *g_pEngine = nullptr;
ICvar *g_pICvar = nullptr;
IServerGameClients *g_pGameClients = nullptr;
IGameEventSystem *g_pGameEventSystem = nullptr;

PLUGIN_EXPOSE(MMSPlugin, g_ThisPlugin);

CON_COMMAND_F(cs2visuals_cycle, "Cycle the CS2 Visuals brightness level",
			  FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE)
{
	const int slot = context.GetPlayerSlot().Get();
	if (slot >= 0)
	{
		g_Brightness.Cycle(slot);
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

	g_pCVar = g_pICvar;
	g_SMAPI->AddListener(this, this);

	SH_ADD_HOOK(IServerGameClients, OnClientConnected, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_OnClientConnected), false);
	SH_ADD_HOOK(IServerGameClients, ClientPutInServer, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientPutInServer), true);
	SH_ADD_HOOK(IServerGameClients, ClientActive, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientActive), true);
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_ADD_HOOK(IServerGameClients, ClientCommand, g_pGameClients,
				SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_ADD_HOOK(ICvar, DispatchConCommand, g_pICvar,
				SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);

	META_CONVAR_REGISTER(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_GAMEDLL);
	META_CONPRINTF("[CS2 Visuals] Loaded v%s; G will cycle brightness levels.\n", GetVersion());
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
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientDisconnect), true);
	SH_REMOVE_HOOK(IServerGameClients, ClientCommand, g_pGameClients,
				   SH_MEMBER(this, &MMSPlugin::Hook_ClientCommand), false);
	SH_REMOVE_HOOK(ICvar, DispatchConCommand, g_pICvar,
				   SH_MEMBER(this, &MMSPlugin::Hook_DispatchConCommand), false);

	g_Brightness.ResetAll();
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
	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_ClientActive(CPlayerSlot slot, bool loadGame, const char *name, uint64 xuid)
{
	(void)loadGame;
	(void)name;
	(void)xuid;
	// KZ servers do not need the original drop action. Bind G to the explicit
	// plugin command so the feature works even when drop is handled internally.
	if (g_pEngine)
	{
		g_pEngine->ClientCommand(slot, "bind g \"cs2visuals_cycle\"");
	}
	CS2VisualsChat(slot.Get(), "[CS2 Visuals] Loaded. Press G to cycle brightness: default -> 1x -> 2x -> 3x.");
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
	const char *command = args.Arg(0);
	if (command && (V_stricmp(command, "drop") == 0 || V_stricmp(command, "cs2visuals_cycle") == 0))
	{
		g_Brightness.Cycle(slot.Get());
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

void MMSPlugin::Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext &context, const CCommand &args)
{
	const char *command = args.Arg(0);
	const int slot = context.GetPlayerSlot().Get();
	if (command && slot >= 0 && (V_stricmp(command, "drop") == 0 || V_stricmp(command, "cs2visuals_cycle") == 0))
	{
		g_Brightness.Cycle(slot.Get());
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}
