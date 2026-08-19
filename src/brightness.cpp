#include "brightness.h"

#include "chat.h"
#include "plugin.h"

#include <chrono>
#include <cstdio>

#include <engine/igameeventsystem.h>
#include <irecipientfilter.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include <netmessages.pb.h>
#include <networkbasetypes.pb.h>
#include <playerslot.h>
#include <tier1/convar.h>

CConVar<bool> g_cs2visualsEnabled("cs2visuals_enabled", FCVAR_NONE,
							  "Enable per-player brightness changes", true);
CConVar<float> g_cs2visualsGammaDefault("cs2visuals_gamma_default", FCVAR_NONE,
											 "Gamma used by the default brightness level", 2.2f);
CConVar<float> g_cs2visualsGamma1("cs2visuals_gamma_1", FCVAR_NONE,
									 "Gamma used by brightness level 1", 2.0f);
CConVar<float> g_cs2visualsGamma2("cs2visuals_gamma_2", FCVAR_NONE,
									 "Gamma used by brightness level 2", 1.8f);
CConVar<float> g_cs2visualsGamma3("cs2visuals_gamma_3", FCVAR_NONE,
									 "Gamma used by brightness level 3", 1.6f);

namespace
{
float ClampGamma(float value)
{
	if (value < 1.0f)
	{
		return 1.0f;
	}
	if (value > 3.0f)
	{
		return 3.0f;
	}
	return value;
}

float GammaForLevel(int level)
{
	switch (level)
	{
		case 1:
			return ClampGamma(g_cs2visualsGamma1.Get());
		case 2:
			return ClampGamma(g_cs2visualsGamma2.Get());
		case 3:
			return ClampGamma(g_cs2visualsGamma3.Get());
		default:
			return ClampGamma(g_cs2visualsGammaDefault.Get());
	}
}

void SendGamma(int slot, float gamma)
{
	char value[32];
	snprintf(value, sizeof(value), "%.3f", gamma);

	if (g_pNetworkMessages && g_pGameEventSystem)
	{
		INetworkMessageInternal *message = g_pNetworkMessages->FindNetworkMessagePartial("SetConVar");
		if (message)
		{
			CNetMessage *raw = message->AllocateMessage();
			if (raw)
			{
				auto *data = raw->ToPB<CNETMsg_SetConVar>();
				CMsg_CVars_CVar *cvar = data->mutable_convars()->add_cvars();
				cvar->set_name("r_fullscreen_gamma");
				cvar->set_value(value);

				class GammaRecipientFilter final : public IRecipientFilter
				{
				public:
					explicit GammaRecipientFilter(int recipient)
					{
						m_recipients.ClearAll();
						m_recipients.Set(recipient);
					}
					NetChannelBufType_t GetNetworkBufType() const override { return BUF_RELIABLE; }
					bool IsInitMessage() const override { return false; }
					const CPlayerBitVec &GetRecipients() const override { return m_recipients; }
					CPlayerSlot GetPredictedPlayerSlot() const override { return CPlayerSlot(-1); }
				private:
					CPlayerBitVec m_recipients;
				} filter(slot);

				g_pGameEventSystem->PostEventAbstract(-1, false, &filter, message, data, 0);
				delete data;
				return;
			}
		}
	}

	// This fallback helps on older server builds that do not expose SetConVar.
	if (g_pEngine)
	{
		g_pEngine->ClientCommand(CPlayerSlot(slot), "r_fullscreen_gamma %s", value);
	}
}
} // namespace

bool BrightnessController::IsValidSlot(int slot) const
{
	return slot >= 0 && slot < ABSOLUTE_PLAYER_LIMIT;
}

void BrightnessController::OnClientConnected(int slot)
{
	if (!IsValidSlot(slot))
	{
		return;
	}
	m_connected[slot] = true;
	m_levels[slot] = 0;
	m_lastCycleTime[slot] = 0;
}

void BrightnessController::OnClientPutInServer(int slot)
{
	if (!IsValidSlot(slot))
	{
		return;
	}
	m_connected[slot] = true;
	m_lastCycleTime[slot] = 0;
	ResetPlayer(slot);
}

void BrightnessController::OnClientDisconnect(int slot)
{
	if (!IsValidSlot(slot))
	{
		return;
	}
	m_connected[slot] = false;
	m_levels[slot] = 0;
	m_lastCycleTime[slot] = 0;
}

void BrightnessController::ResetPlayer(int slot)
{
	m_levels[slot] = 0;
	ApplyLevel(slot);
}

void BrightnessController::ResetAll()
{
	for (int slot = 0; slot < ABSOLUTE_PLAYER_LIMIT; ++slot)
	{
		m_levels[slot] = 0;
		m_lastCycleTime[slot] = 0;
		if (m_connected[slot])
		{
			ApplyLevel(slot);
		}
	}
}

void BrightnessController::Cycle(int slot)
{
	if (!IsValidSlot(slot) || !m_connected[slot] || !g_cs2visualsEnabled.Get())
	{
		return;
	}

	// A single client command can pass through both ClientCommand and
	// DispatchConCommand on some CS2/KZ combinations. Treat those same-tick
	// callbacks as one key press while preserving normal rapid key presses.
	const uint64_t now = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
	if (now >= m_lastCycleTime[slot] && now - m_lastCycleTime[slot] < 50)
	{
		return;
	}
	m_lastCycleTime[slot] = now;

	m_levels[slot] = (m_levels[slot] + 1) % 4;
	ApplyLevel(slot);

	if (m_levels[slot] == 0)
	{
		CS2VisualsChat(slot, "[CS2 Visuals] Brightness: default");
	}
	else
	{
		CS2VisualsChat(slot, "[CS2 Visuals] Brightness: %dx", m_levels[slot]);
	}
}

int BrightnessController::GetLevel(int slot) const
{
	return IsValidSlot(slot) ? m_levels[slot] : 0;
}

void BrightnessController::ApplyLevel(int slot)
{
	if (!IsValidSlot(slot))
	{
		return;
	}
	SendGamma(slot, GammaForLevel(m_levels[slot]));
}
