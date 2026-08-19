#ifndef _INCLUDE_CS2VISUALS_POSTPROCESS_H_
#define _INCLUDE_CS2VISUALS_POSTPROCESS_H_

#include <const.h>
#include <entity2/entitysystem.h>
#include <entityhandle.h>

class CCheckTransmitInfo;
class CEntityKeyValues;

class PostProcessController
{
public:
	bool Initialize(char *error, size_t maxlen);
	void Shutdown();

	bool IsReady() const { return m_ready; }
	void SetLevel(int slot, int level);
	void RemovePlayer(int slot);
	void RemoveAll();
	void ClearForMapChange();
	void ProcessPending();

	void FilterTransmit(CCheckTransmitInfo **infos, int count);

private:
	struct PlayerEffect
	{
		CEntityHandle entity;
		int level = 0;
	};

	CGameEntitySystem *GetEntitySystem() const;
	CEntityInstance *CreateEffect(const char *resource);
	void DestroyEffect(PlayerEffect &effect);
	const char *ResourceForLevel(int level) const;

	PlayerEffect m_players[ABSOLUTE_PLAYER_LIMIT]{};
	int m_pendingLevels[ABSOLUTE_PLAYER_LIMIT]{};
	bool m_pending[ABSOLUTE_PLAYER_LIMIT]{};
	bool m_ready = false;
};

extern PostProcessController g_PostProcess;

#endif // _INCLUDE_CS2VISUALS_POSTPROCESS_H_
