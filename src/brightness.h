#ifndef _INCLUDE_CS2VISUALS_BRIGHTNESS_H_
#define _INCLUDE_CS2VISUALS_BRIGHTNESS_H_

#include <const.h>

class BrightnessController
{
public:
	void OnClientConnected(int slot);
	void OnClientPutInServer(int slot);
	void OnClientDisconnect(int slot);

	void Cycle(int slot);
	void ResetAll();
	int GetLevel(int slot) const;

private:
	void ResetPlayer(int slot);
	void ApplyLevel(int slot);
	bool IsValidSlot(int slot) const;

	int m_levels[ABSOLUTE_PLAYER_LIMIT]{};
	bool m_connected[ABSOLUTE_PLAYER_LIMIT]{};
};

#endif // _INCLUDE_CS2VISUALS_BRIGHTNESS_H_
