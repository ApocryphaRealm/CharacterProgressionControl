#include "PCH.h"

#include "MenuStrings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace MenuStrings
{
	namespace
	{
		// The settings page's own namespace. utils/Strings.h reads the SAME file into the page's
		// own map, so these keys never belonged in the GAME's translation table - that table is a
		// global namespace shared with the vanilla menus and every other mod. 1.1.3 pushed 310 of
		// them into it when the language rollout merged the page's keys into this file (the file
		// grew from 5,072 bytes to 54,304), which is what exhausted the table. See below.
		const std::wstring kPagePrefix = L"$CPC_";

		[[nodiscard]] bool IsPageKey(const std::wstring& a_key)
		{
			return a_key.compare(0, kPagePrefix.size(), kPagePrefix) == 0;
		}

		// A view of the game's translation table header, for IDENTIFYING it before touching it.
		// Field names are deliberately offsets, not meanings, because the meanings are not settled:
		//
		//   SKSE64 (skse64\GameTypes.h, tHashSet) documents m_size 0x0C, m_freeCount 0x10,
		//   m_freeOffset 0x14, m_eolPtr 0x18, m_entries 0x28, and CommonLibSSE-NG agrees, reading
		//   capacity at 0x0C and the free count at 0x10. Measured on a 945-mod AE 1.6.1170 list on
		//   2026-09-08, those fields read 586 and 6460 - the "capacity" is not a power of two and the
		//   "free count" exceeds it, so at least one of them is not what either library thinks.
		//
		// That matters because CommonLibSSE indexes the table as entries[hash & (capacity - 1)]. With
		// a capacity that is not a power of two the mask is wrong, the index can fall outside the
		// entries array, and the lookup dereferences whatever is there. That is how 1.1.4's first
		// attempt crashed in find() before inserting anything, and it is the same shape as the 1.1.3
		// report from a user (chain pointer 0x100000020).
		//
		// Until the true layout is established on a running game, this mod does not write to the
		// table unless the header positively matches the structure it models. Untranslated menu
		// labels are a cosmetic fault; corrupting an engine structure is not.
		struct TableHeader
		{
			std::uint64_t unk00;       // 00
			std::uint32_t at08;        // 08
			std::uint32_t at0C;        // 0C - CommonLibSSE reads the capacity here
			std::uint32_t at10;        // 10 - CommonLibSSE reads the free count here
			std::uint32_t at14;        // 14
			const void*   eolPtr;      // 18 - end-of-list sentinel
			std::uint64_t unk20;       // 20
			const void*   entries;     // 28
		};
		static_assert(sizeof(TableHeader) == 0x30);

		// Never take the last slots: leave the game room for its own strings, and keep a margin
		// between us and the reallocation path we must never enter.
		constexpr std::uint32_t kFreeSlotMargin = 16;

		[[nodiscard]] bool IsPowerOfTwo(std::uint32_t a_v) { return a_v != 0 && (a_v & (a_v - 1)) == 0; }

		// Refuse to touch the table at all unless it looks exactly like the structure we model.
		// CommonLibSSE indexes it as entries[hash & (capacity - 1)], which is only meaningful when
		// the capacity really is a power of two at the offset CommonLibSSE reads it from. On
		// 2026-09-08 that field read 586 on a 945-mod AE 1.6.1170 list - not a power of two - and
		// the resulting index walked outside the entries array and crashed in find(). Whatever the
		// true layout turns out to be, a mod must not write into an engine structure it cannot
		// positively identify, so this check gates everything below it and the log records what was
		// actually seen.
		[[nodiscard]] bool LooksSane(const TableHeader& a_h)
		{
			if (!a_h.entries || !a_h.eolPtr) { return false; }
			if (!IsPowerOfTwo(a_h.at0C)) { return false; }
			if (a_h.at10 > a_h.at0C) { return false; }
			return true;
		}

		std::string Language()
		{
			std::string lang = "english";
			if (auto* ini = RE::INISettingCollection::GetSingleton())
			{
				if (auto* setting = ini->GetSetting("sLanguage:General"); setting && setting->GetString() && setting->GetString()[0])
				{
					lang = setting->GetString();
				}
			}
			std::transform(lang.begin(), lang.end(), lang.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lang;
		}

		std::vector<std::pair<std::wstring, std::wstring>> ReadFile(const std::filesystem::path& a_path)
		{
			std::vector<std::pair<std::wstring, std::wstring>> out;
			std::ifstream in(a_path, std::ios::binary);
			if (!in) { return out; }
			std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (bytes.size() < 2 || static_cast<unsigned char>(bytes[0]) != 0xFF || static_cast<unsigned char>(bytes[1]) != 0xFE) { return out; }
			std::wstring text(reinterpret_cast<const wchar_t*>(bytes.data() + 2), (bytes.size() - 2) / 2);
			std::size_t pos = 0;
			while (pos < text.size())
			{
				auto eol = text.find(L'\n', pos);
				if (eol == std::wstring::npos) { eol = text.size(); }
				std::wstring line = text.substr(pos, eol - pos);
				pos = eol + 1;
				if (!line.empty() && line.back() == L'\r') { line.pop_back(); }
				if (line.empty() || line[0] != L'$') { continue; }
				const auto tab = line.find(L'\t');
				if (tab == std::wstring::npos) { continue; }
				out.emplace_back(line.substr(0, tab), line.substr(tab + 1));
			}
			return out;
		}
	}

	void Install()
	{
		const auto lang = Language();
		auto path = std::filesystem::current_path() / "Data" / "Interface" / "Translations" / ("CharacterProgressionControl_" + lang + ".txt");
		auto entries = ReadFile(path);
		logger::debug("menu strings: read {} entries from {} ({})", entries.size(), path.string(), lang);
		if (entries.empty() && lang != "english")
		{
			path = std::filesystem::current_path() / "Data" / "Interface" / "Translations" / "CharacterProgressionControl_english.txt";
			entries = ReadFile(path);
			logger::debug("menu strings: {} not readable, fell back to english and read {} entries", lang, entries.size());
		}
		if (entries.empty()) { logger::warn("menu strings: no translation file read at {}; the level-up menu will show raw keys", path.string()); return; }
		auto* manager = RE::BSScaleformManager::GetSingleton();
		auto* loader = manager ? manager->loader : nullptr;
		auto translator = loader ? loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator) : RE::GPtr<RE::BSScaleformTranslator>{};
		if (!translator) { logger::warn("menu strings: the game's Scaleform translator was not reachable; the level-up menu will show raw keys"); return; }

		auto&       map    = translator->translator.translationMap;
		const auto* header = reinterpret_cast<const TableHeader*>(&map);
		logger::info("menu strings: translation table header - +08={} +0C={} +10={} +14={} entries={} sentinel={}",
			header->at08, header->at0C, header->at10, header->at14,
			fmt::ptr(header->entries), fmt::ptr(header->eolPtr));

		if (!LooksSane(*header))
		{
			logger::warn("menu strings: the game's translation table does not match the layout this mod knows how to read "
			             "(capacity field {} is not a power of two, or the pointers are missing) - not touching it. "
			             "The level-up menu will show its raw $keys instead of translated labels, which is a cosmetic "
			             "fault; writing to a table we cannot identify would corrupt the game.", header->at0C);
			return;
		}

		int added = 0, present = 0, pageKeys = 0, refused = 0;
		for (const auto& [key, value] : entries)
		{
			// The page reads these itself; the engine's table must never see them.
			if (IsPageKey(key)) { ++pageKeys; continue; }

			// Stop before the table can fill - entering CommonLibSSE's reserve() on a table the
			// GAME allocated is the 1.1.3 crash.
			if (header->at10 <= kFreeSlotMargin) { ++refused; continue; }

			RE::BSFixedStringW k(key.c_str());
			if (map.find(k) != map.end()) { ++present; continue; }
			map.emplace(k, RE::BSFixedStringW(value.c_str()));
			++added;
		}

		if (refused > 0)
		{
			logger::warn("menu strings: the game's translation table had no room for {} entries ({} slots, {} free); those labels will show raw keys", refused, header->at0C, header->at10);
		}
		logger::info("menu strings: {} added, {} already present, {} page keys left to the settings page, {} refused for space, from {} ({}); table now {} slots with {} free",
			added, present, pageKeys, refused, path.filename().string(), lang, header->at0C, header->at10);
	}
}
