#ifndef _INCLUDE_CS2VISUALS_PLUGIN_H_
#define _INCLUDE_CS2VISUALS_PLUGIN_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iserver.h>
#include <sh_vector.h>

#include "brightness.h"
#include "postprocess.h"
#include "version_gen.h"

class INetworkMessages;
class IGameEventSystem;
class ISource2Server;

class MMSPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();

	void OnLevelInit(char const *mapName, char const *mapEntities, char const *oldLevel, char const *landmarkName,
					bool loadGame, bool background);
	void OnLevelShutdown();

	void Hook_OnClientConnected(CPlayerSlot slot, const char *name, uint64 xuid, const char *networkId,
								const char *address, bool fakePlayer);
	void Hook_ClientPutInServer(CPlayerSlot slot, const char *name, int type, uint64 xuid);
	void Hook_ClientActive(CPlayerSlot slot, bool loadGame, const char *name, uint64 xuid);
	void Hook_ClientFullyConnect(CPlayerSlot slot);
	void Hook_ClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *name,
								   uint64 xuid, const char *networkId);
	void Hook_ClientCommand(CPlayerSlot slot, const CCommand &args);
	void Hook_DispatchConCommand(ConCommandRef cmd, const CCommandContext &context, const CCommand &args);
	void Hook_CheckTransmit(CCheckTransmitInfo **infos, int count, CBitVec<16384> &unionTransmitEdicts,
		CBitVec<16384> &unknown, const Entity2Networkable_t **networkables,
		const uint16 *entityIndices, int entityCount);
	void Hook_GameFrame(bool simulating, bool firstTick, bool lastTick);

	const char *GetAuthor() { return PLUGIN_AUTHOR; }
	const char *GetName() { return PLUGIN_DISPLAY_NAME; }
	const char *GetDescription() { return PLUGIN_DESCRIPTION; }
	const char *GetURL() { return PLUGIN_URL; }
	const char *GetLicense() { return PLUGIN_LICENSE; }
	const char *GetVersion() { return PLUGIN_FULL_VERSION; }
	const char *GetDate() { return __DATE__; }
	const char *GetLogTag() { return PLUGIN_LOGTAG; }
};

extern MMSPlugin g_ThisPlugin;
extern BrightnessController g_Brightness;

extern IVEngineServer *g_pEngine;
extern ICvar *g_pICvar;
extern IServerGameClients *g_pGameClients;
extern INetworkMessages *g_pNetworkMessages;
extern IGameEventSystem *g_pGameEventSystem;
extern ISource2Server *g_pSource2Server;

PLUGIN_GLOBALVARS();

#endif // _INCLUDE_CS2VISUALS_PLUGIN_H_
