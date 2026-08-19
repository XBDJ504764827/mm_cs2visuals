#include "postprocess.h"

#include "plugin.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <checktransmitinfo.h>
#include <entity2/entitykeyvalues.h>
#include <interfaces/interfaces.h>
#include <tier1/convar.h>

#if defined(_WIN32)
#include <windows.h>
#include <winnt.h>
#else
#include <dlfcn.h>
#include <link.h>
#endif

// The presets are provided by the CS2KR night-vision Workshop addon. The
// server sends the .vpost name; CS2's resource system resolves the _c file.
CConVar<CUtlString> g_cs2visualsPostProcess1(
	"cs2visuals_postprocess_1", FCVAR_NONE,
	"Post-processing resource for brightness level 1",
	"lighting/postprocessing/cs2kr/nightvision/nv_soft.vpost");
CConVar<CUtlString> g_cs2visualsPostProcess2(
	"cs2visuals_postprocess_2", FCVAR_NONE,
	"Post-processing resource for brightness level 2",
	"lighting/postprocessing/cs2kr/nightvision/nv_medium.vpost");
CConVar<CUtlString> g_cs2visualsPostProcess3(
	"cs2visuals_postprocess_3", FCVAR_NONE,
	"Post-processing resource for brightness level 3",
	"lighting/postprocessing/cs2kr/nightvision/nv_strong.vpost");

namespace
{
using CreateEntityByNameFn = CEntityInstance *(*)(const char *, int);
using DispatchSpawnFn = void (*)(CEntityInstance *, CEntityKeyValues *);
using RemoveEntityFn = void (*)(CEntityInstance *);

CreateEntityByNameFn g_createEntity = nullptr;
DispatchSpawnFn g_dispatchSpawn = nullptr;
RemoveEntityFn g_removeEntity = nullptr;

struct PatternByte
{
	uint8_t value;
	bool wildcard;
};

bool ParsePattern(const char *text, std::vector<PatternByte> &out)
{
	out.clear();
	while (*text)
	{
		while (*text && std::isspace(static_cast<unsigned char>(*text)))
			++text;
		if (!*text)
			break;

		if (*text == '?' || (*text == '2' && text[1] == 'A'))
		{
			if (*text == '?')
				++text;
			else
				text += 2;
			while (*text && !std::isspace(static_cast<unsigned char>(*text)))
				++text;
			out.push_back({0, true});
			continue;
		}

		char *end = nullptr;
		const unsigned long value = std::strtoul(text, &end, 16);
		if (end == text || value > 0xff)
			return false;
		out.push_back({static_cast<uint8_t>(value), false});
		text = end;
	}
	return !out.empty();
}

void *FindPattern(uint8_t *base, size_t size, const char *pattern)
{
	std::vector<PatternByte> bytes;
	if (!ParsePattern(pattern, bytes) || bytes.size() > size)
		return nullptr;

	for (size_t offset = 0; offset + bytes.size() <= size; ++offset)
	{
		bool match = true;
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			if (!bytes[i].wildcard && base[offset + i] != bytes[i].value)
			{
				match = false;
				break;
			}
		}
		if (match)
			return base + offset;
	}
	return nullptr;
}

#if defined(_WIN32)
bool GetServerText(uint8_t **base, size_t *size)
{
	HMODULE module = GetModuleHandleA("server.dll");
	if (!module)
		return false;
	const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return false;
	const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<const uint8_t *>(module) + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return false;
	*base = reinterpret_cast<uint8_t *>(module);
	*size = nt->OptionalHeader.SizeOfImage;
	return true;
}
#else
struct ModuleSearch
{
	uint8_t *base = nullptr;
	size_t size = 0;
};

int FindServerText(struct dl_phdr_info *info, size_t, void *data)
{
	const char *name = info->dlpi_name;
	if (!name || !*name || (!std::strstr(name, "/server.so") && std::strcmp(name, "server.so") != 0))
		return 0;

	auto *result = static_cast<ModuleSearch *>(data);
	for (int i = 0; i < info->dlpi_phnum; ++i)
	{
		const ElfW(Phdr) &phdr = info->dlpi_phdr[i];
		if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X))
		{
			result->base = reinterpret_cast<uint8_t *>(info->dlpi_addr + phdr.p_vaddr);
			result->size = phdr.p_memsz;
			return 1;
		}
	}
	return 0;
}

bool GetServerText(uint8_t **base, size_t *size)
{
	ModuleSearch result;
	dl_iterate_phdr(FindServerText, &result);
	*base = result.base;
	*size = result.size;
	return result.base != nullptr && result.size != 0;
}
#endif

bool ResolveEntityFunctions(char *error, size_t maxlen)
{
	uint8_t *base = nullptr;
	size_t size = 0;
	if (!GetServerText(&base, &size))
	{
		V_snprintf(error, maxlen, "Could not locate the loaded CS2 server module.");
		return false;
	}

#if defined(_WIN32)
	g_createEntity = reinterpret_cast<CreateEntityByNameFn>(FindPattern(base, size, "48 83 EC 48 C6 44 24 30 00"));
	g_dispatchSpawn = reinterpret_cast<DispatchSpawnFn>(FindPattern(base, size, "48 89 5C 24 ?? 57 48 83 EC ?? 48 8B DA 48 8B F9 48 85 C9 0F 84 ?? ?? ?? ?? 48 85 D2"));
	g_removeEntity = reinterpret_cast<RemoveEntityFn>(FindPattern(base, size, "48 85 C9 74 ?? 48 8B D1 48 8B 0D ?? ?? ?? ??"));
#else
	// Signatures match the current CS2 server binaries used by CS2KZ and CS2Fixes.
	g_createEntity = reinterpret_cast<CreateEntityByNameFn>(FindPattern(base, size, "48 8D 05 ?? ?? ?? ?? 55 48 89 FA"));
	g_dispatchSpawn = reinterpret_cast<DispatchSpawnFn>(FindPattern(base, size, "48 85 FF 74 ?? 55 48 89 E5 41 55 41 54 49 89 FC"));
	g_removeEntity = reinterpret_cast<RemoveEntityFn>(FindPattern(base, size, "48 89 FE 48 85 FF 74 ?? 48 8D 05 ?? ?? ?? ?? 48"));
#endif

	if (!g_createEntity || !g_dispatchSpawn || !g_removeEntity)
	{
		V_snprintf(error, maxlen, "Could not resolve CS2 entity functions (CreateEntityByName/DispatchSpawn/RemoveEntity).");
		return false;
	}
	return true;
}

int EntitySystemOffset()
{
#if defined(_WIN32)
	return 88;
#else
	return 80;
#endif
}
} // namespace

PostProcessController g_PostProcess;

CGameEntitySystem *GameEntitySystem()
{
	if (!g_pGameResourceServiceServer)
		return nullptr;
	return *reinterpret_cast<CGameEntitySystem **>(reinterpret_cast<uintptr_t>(g_pGameResourceServiceServer) + EntitySystemOffset());
}

CGameEntitySystem *PostProcessController::GetEntitySystem() const
{
	return GameEntitySystem();
}

bool PostProcessController::Initialize(char *error, size_t maxlen)
{
	if (!g_pGameResourceServiceServer || !g_pSource2GameEntities)
	{
		V_snprintf(error, maxlen, "Required Source 2 entity interfaces are unavailable.");
		return false;
	}
	if (!ResolveEntityFunctions(error, maxlen))
		return false;
	m_ready = true;
	return true;
}

void PostProcessController::Shutdown()
{
	RemoveAll();
	m_ready = false;
	g_createEntity = nullptr;
	g_dispatchSpawn = nullptr;
	g_removeEntity = nullptr;
}

const char *PostProcessController::ResourceForLevel(int level) const
{
	switch (level)
	{
		case 1: return g_cs2visualsPostProcess1.Get().Get();
		case 2: return g_cs2visualsPostProcess2.Get().Get();
		case 3: return g_cs2visualsPostProcess3.Get().Get();
		default: return nullptr;
	}
}

CEntityInstance *PostProcessController::CreateEffect(const char *resource)
{
	if (!m_ready || !resource || !*resource || !GetEntitySystem())
		return nullptr;

	CEntityInstance *entity = g_createEntity("post_processing_volume", -1);
	if (!entity)
		return nullptr;

	// DispatchSpawn consumes the key values during entity construction. Keep
	// this allocation alive after the call, matching the Source 2 server helper
	// used by CS2KZ and CS2Fixes.
	auto *keyValues = new CEntityKeyValues();
	keyValues->SetBool("master", true);
	keyValues->SetFloat("fadetime", 0.0f);
	keyValues->SetString("postprocessing", resource);
	g_dispatchSpawn(entity, keyValues);
	return entity;
}

void PostProcessController::DestroyEffect(PlayerEffect &effect)
{
	if (effect.entity.IsValid())
	{
		CGameEntitySystem *entitySystem = GetEntitySystem();
		CEntityInstance *entity = entitySystem ? entitySystem->GetEntityInstance(effect.entity) : nullptr;
		if (entity && g_removeEntity)
			g_removeEntity(entity);
	}
	effect.entity.Term();
	effect.level = 0;
}

void PostProcessController::SetLevel(int slot, int level)
{
	if (slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
		return;
	level = std::clamp(level, 0, 3);
	m_pendingLevels[slot] = level;
	m_pending[slot] = true;
}

void PostProcessController::RemovePlayer(int slot)
{
	if (slot < 0 || slot >= ABSOLUTE_PLAYER_LIMIT)
		return;
	m_pending[slot] = false;
	DestroyEffect(m_players[slot]);
}

void PostProcessController::RemoveAll()
{
	for (int slot = 0; slot < ABSOLUTE_PLAYER_LIMIT; ++slot)
	{
		m_pending[slot] = false;
		DestroyEffect(m_players[slot]);
	}
}

void PostProcessController::ClearForMapChange()
{
	// LoopShutdown runs after the map entity system has begun tearing down. Do
	// not call the server RemoveEntity helper here; just invalidate stale
	// handles and let the old world reclaim its entities.
	for (int slot = 0; slot < ABSOLUTE_PLAYER_LIMIT; ++slot)
	{
		m_pending[slot] = false;
		m_pendingLevels[slot] = 0;
		m_players[slot].entity.Term();
		m_players[slot].level = 0;
	}
}

void PostProcessController::ProcessPending()
{
	if (!m_ready)
		return;

	for (int slot = 0; slot < ABSOLUTE_PLAYER_LIMIT; ++slot)
	{
		if (!m_pending[slot])
			continue;
		const int level = m_pendingLevels[slot];
		m_pending[slot] = false;
		DestroyEffect(m_players[slot]);
		if (level == 0)
			continue;

		const char *resource = ResourceForLevel(level);
		CEntityInstance *entity = CreateEffect(resource);
		if (!entity)
		{
			META_CONPRINTF("[CS2 Visuals] Failed to create post-processing entity for slot %d (level %d, resource %s).\n",
				slot, level, resource && *resource ? resource : "<empty>");
			continue;
		}
		m_players[slot].entity = entity;
		m_players[slot].level = level;
		META_CONPRINTF("[CS2 Visuals] Slot %d brightness level %d uses %s.\n", slot, level,
			resource && *resource ? resource : "<empty>");
	}
}

void PostProcessController::FilterTransmit(CCheckTransmitInfo **infos, int count)
{
	if (!m_ready || !infos)
		return;

	for (int i = 0; i < count; ++i)
	{
		CCheckTransmitInfo *info = infos[i];
		if (!info || !info->m_pTransmitEntity)
			continue;
		const int recipient = *reinterpret_cast<const int *>(reinterpret_cast<const uint8_t *>(info) + 576);
		if (recipient < 0 || recipient >= ABSOLUTE_PLAYER_LIMIT)
			continue;

		for (int owner = 0; owner < ABSOLUTE_PLAYER_LIMIT; ++owner)
		{
			const PlayerEffect &effect = m_players[owner];
			if (!effect.entity.IsValid() || effect.level == 0 || owner == recipient)
				continue;
			info->m_pTransmitEntity->Clear(effect.entity.GetEntryIndex());
		}
	}
}
