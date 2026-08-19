#include "brightness.h"

#include "chat.h"
#include "postprocess.h"
#include "plugin.h"

#include <chrono>
#include <tier1/convar.h>

CConVar<bool> g_cs2visualsEnabled("cs2visuals_enabled", FCVAR_NONE,
							  "Enable per-player brightness changes", true);

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
	g_PostProcess.RemovePlayer(slot);
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
	g_PostProcess.RemovePlayer(slot);
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
		g_PostProcess.RemovePlayer(slot);
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
	g_PostProcess.SetLevel(slot, m_levels[slot]);
}
