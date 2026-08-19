#include "chat.h"

#include "plugin.h"

#include <cstdarg>
#include <cstdio>

#include <const.h>
#include <engine/igameeventsystem.h>
#include <irecipientfilter.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include <playerslot.h>
#include <shareddefs.h>
#include <usermessages.pb.h>

namespace
{
class SingleRecipientFilter final : public IRecipientFilter
{
public:
	explicit SingleRecipientFilter(int slot)
	{
		m_recipients.ClearAll();
		if (slot >= 0 && slot < ABSOLUTE_PLAYER_LIMIT)
		{
			m_recipients.Set(slot);
		}
	}

	NetChannelBufType_t GetNetworkBufType() const override { return BUF_RELIABLE; }
	bool IsInitMessage() const override { return false; }
	const CPlayerBitVec &GetRecipients() const override { return m_recipients; }
	CPlayerSlot GetPredictedPlayerSlot() const override { return CPlayerSlot(-1); }

private:
	CPlayerBitVec m_recipients;
};
} // namespace

void CS2VisualsChat(int slot, const char *format, ...)
{
	if (!g_pNetworkMessages || !g_pGameEventSystem || slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
	{
		return;
	}

	char text[256];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	INetworkMessageInternal *message = g_pNetworkMessages->FindNetworkMessagePartial("TextMsg");
	if (!message)
	{
		return;
	}

	CNetMessage *raw = message->AllocateMessage();
	if (!raw)
	{
		return;
	}

	auto *data = raw->ToPB<CUserMessageTextMsg>();
	data->set_dest(HUD_PRINTTALK);
	data->add_param(text);

	SingleRecipientFilter filter(slot);
	g_pGameEventSystem->PostEventAbstract(-1, false, &filter, message, data, 0);
	delete data;
}
