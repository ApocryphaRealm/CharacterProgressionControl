#include "PCH.h"

#include "MenuStrings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace MenuStrings
{
	namespace
	{
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
		if (entries.empty() && lang != "english")
		{
			path = std::filesystem::current_path() / "Data" / "Interface" / "Translations" / "CharacterProgressionControl_english.txt";
			entries = ReadFile(path);
		}
		if (entries.empty()) { logger::warn("menu strings: no translation file read at {}; the level-up menu will show raw keys", path.string()); return; }
		auto* manager = RE::BSScaleformManager::GetSingleton();
		auto* loader = manager ? manager->loader : nullptr;
		auto translator = loader ? loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator) : RE::GPtr<RE::BSScaleformTranslator>{};
		if (!translator) { logger::warn("menu strings: the game's Scaleform translator was not reachable; the level-up menu will show raw keys"); return; }
		int added = 0;
		for (const auto& [key, value] : entries)
		{
			RE::BSFixedStringW k(key.c_str());
			if (translator->translator.translationMap.find(k) != translator->translator.translationMap.end()) { continue; }
			translator->translator.translationMap.emplace(k, RE::BSFixedStringW(value.c_str()));
			++added;
		}
		logger::info("menu strings: {} of {} entries added to the translator from {} ({})", added, entries.size(), path.filename().string(), lang);
	}
}
