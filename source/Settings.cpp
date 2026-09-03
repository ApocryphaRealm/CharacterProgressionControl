#include "PCH.h"

#include "Settings.h"

#include "utils/INISettingCollection.h"
#include "utils/Logger.h"
#include "utils/Setting.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace settings
{
	namespace
	{
		std::string iniPath;

		struct Defaults
		{
			std::uint32_t logLevel;
			bool overrideCost;
			float base;
			float mult;
			bool overrideCaps;
			float cap[skilllist::kCount];
			float formulaCap[skilllist::kCount];
			bool overrideEnchanting;
			float ench[4];
		} defaults{};

		// Vanilla Skyrim stops every skill at 100, and that is what "default" means here.
		constexpr float kVanillaSkillCap = 100.0F;

		std::string SkillKey(const char* a_prefix, int a_index)
		{
			return std::string(a_prefix) + skilllist::kIniName[a_index];
		}

		std::string Lower(std::string a_s)
		{
			for (char& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return a_s;
		}

		std::string Trim(const std::string& a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) { return {}; }
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return a_s.substr(b, e - b + 1);
		}

		bool ParseBool(const std::string& a_text, bool& a_out)
		{
			const std::string v = Lower(Trim(a_text));
			if (v == "1" || v == "true" || v == "yes") { a_out = true; return true; }
			if (v == "0" || v == "false" || v == "no") { a_out = false; return true; }
			return false;
		}

		bool ParseFloat(const std::string& a_text, float& a_out)
		{
			try { a_out = std::stof(Trim(a_text)); return true; } catch (...) { return false; }
		}

		bool ParseUInt(const std::string& a_text, std::uint32_t& a_out)
		{
			// Base auto-detection (0x hex and decimal both work) - the DEM 1.5.8 lesson.
			try { a_out = static_cast<std::uint32_t>(std::stoull(Trim(a_text), nullptr, 0)); return true; } catch (...) { return false; }
		}

		std::map<std::string, std::string> ReadKeys(const std::string& a_path)
		{
			std::map<std::string, std::string> keys;
			std::ifstream in(a_path);
			if (!in) { return keys; }
			std::string line, section;
			while (std::getline(in, line))
			{
				const std::string t = Trim(line);
				if (t.empty() || t[0] == ';' || t[0] == '#') { continue; }
				if (t.front() == '[' && t.back() == ']') { section = Lower(t.substr(1, t.size() - 2)); continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos) { continue; }
				keys[Lower(Trim(t.substr(0, eq))) + ":" + section] = Trim(t.substr(eq + 1));
			}
			return keys;
		}

		bool LoadFileValues(const std::string& a_path)
		{
			if (!std::filesystem::exists(a_path))
			{
				logger::warn("settings file not found at {}; keeping the values in hand", a_path);
				return false;
			}
			const auto k = ReadKeys(a_path);
			bool sawBase = false, sawMult = false;
			auto get = [&](const char* a_key, auto& a_out, auto a_parse, bool* a_saw = nullptr) {
				const auto it = k.find(a_key);
				if (it == k.end()) { logger::debug("INI key {} missing; keeping current value", a_key); return; }
				if (!a_parse(it->second, a_out)) { logger::warn("INI value \"{}\" for {} is not valid; keeping current value", it->second, a_key); return; }
				if (a_saw) { *a_saw = true; }
			};
			get("uloglevel:debug", debug::logLevel, ParseUInt);
			get("boverridelevelcost:levelling", levelling::overrideCost, ParseBool);
			get("flevelupbase:levelling", levelling::base, ParseFloat, &sawBase);
			get("flevelupmult:levelling", levelling::mult, ParseFloat, &sawMult);

			bool sawEnch = false;
			get("boverrideenchanting:enchanting", enchanting::overrideCost, ParseBool);
			get("fenchantingcostbase:enchanting", enchanting::costBase, ParseFloat, &sawEnch);
			get("fenchantingcostscale:enchanting", enchanting::costScale, ParseFloat);
			get("fenchantingcostmult:enchanting", enchanting::costMult, ParseFloat);
			get("fenchantingcostexponent:enchanting", enchanting::costExponent, ParseFloat);
			if (sawEnch) { enchanting::seeded = true; }

			get("boverrideskillcaps:skills", skills::overrideCaps, ParseBool);
			for (int i = 0; i < skilllist::kCount; ++i)
			{
				get((Lower(SkillKey("fCap", i)) + ":skills").c_str(), skills::cap[i], ParseFloat);
				get((Lower(SkillKey("fFormulaCap", i)) + ":skills").c_str(), skills::formulaCap[i], ParseFloat);
			}

			// A configuration read from the file counts as seeded, so the capture at kDataLoaded
			// leaves it alone rather than replacing it with the game's own numbers.
			if (sawBase && sawMult) { levelling::seeded = true; }

			logger::info("settings loaded from {}: overrideCost={} base={:.1f} mult={:.1f} logLevel={}",
						 a_path, levelling::overrideCost, levelling::base, levelling::mult, debug::logLevel);
			return true;
		}

		// Replaces the key in place when it is there, and otherwise ADDS it - at the end of its
		// section, or in a new section at the end of the file. A settings file that predates a
		// new option is the normal case as this mod grows tab by tab, so a missing key must be
		// something Save fixes rather than something it fails on.
		bool WriteKey(std::vector<std::string>& a_lines, const char* a_section, const char* a_key, const std::string& a_value)
		{
			const std::string wantSection = Lower(a_section);
			const std::string wantKey = Lower(a_key);

			std::string section;
			std::size_t sectionEnd = 0;      // one past the last line belonging to the section
			bool sawSection = false;

			for (std::size_t i = 0; i < a_lines.size(); ++i)
			{
				const std::string t = Trim(a_lines[i]);
				if (!t.empty() && t.front() == '[' && t.back() == ']')
				{
					section = Lower(t.substr(1, t.size() - 2));
					if (section == wantSection) { sawSection = true; sectionEnd = i + 1; }
					continue;
				}
				if (section != wantSection) { continue; }
				sectionEnd = i + 1;
				const auto eq = t.find('=');
				if (eq == std::string::npos) { continue; }
				if (Lower(Trim(t.substr(0, eq))) == wantKey)
				{
					a_lines[i] = std::string(a_key) + "=" + a_value;
					return true;
				}
			}

			const std::string entry = std::string(a_key) + "=" + a_value;
			if (sawSection)
			{
				a_lines.insert(a_lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd), entry);
			}
			else
			{
				if (!a_lines.empty() && !Trim(a_lines.back()).empty()) { a_lines.push_back(""); }
				a_lines.push_back("[" + std::string(a_section) + "]");
				a_lines.push_back(entry);
			}
			logger::debug("Save: added {} to [{}]", a_key, a_section);
			return true;
		}

		std::string FormatFloat(float a_v)
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%.1f", a_v);
			return buf;
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		iniPath = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / a_iniFileName).string();

		for (int i = 0; i < skilllist::kCount; ++i)
		{
			skills::cap[i] = kVanillaSkillCap;
			skills::formulaCap[i] = kVanillaSkillCap;
		}

		defaults.logLevel = debug::logLevel;
		defaults.overrideCost = levelling::overrideCost;
		defaults.base = levelling::base;
		defaults.mult = levelling::mult;
		defaults.overrideCaps = skills::overrideCaps;
		for (int i = 0; i < skilllist::kCount; ++i)
		{
			defaults.cap[i] = skills::cap[i];
			defaults.formulaCap[i] = skills::formulaCap[i];
		}
		defaults.overrideEnchanting = enchanting::overrideCost;
		defaults.ench[0] = enchanting::costBase;
		defaults.ench[1] = enchanting::costScale;
		defaults.ench[2] = enchanting::costMult;
		defaults.ench[3] = enchanting::costExponent;

		// Registered for engine-side consistency; NEVER read through the collection - the
		// plain-file parse above is the value of record (redirector-proof).
		auto* collection = utils::INISettingCollection::GetSingleton();
		collection->AddSettings(
			utils::MakeSetting("uLogLevel:Debug", static_cast<unsigned int>(debug::logLevel)),
			utils::MakeSetting("bOverrideLevelCost:Levelling", levelling::overrideCost),
			utils::MakeSetting("fLevelUpBase:Levelling", levelling::base),
			utils::MakeSetting("fLevelUpMult:Levelling", levelling::mult));

		LoadFileValues(iniPath);
	}

	bool Reload()
	{
		const bool ok = LoadFileValues(iniPath);
		ApplyLogLevel();
		return ok;
	}

	bool LoadFrom(const std::string& a_path)
	{
		const bool ok = LoadFileValues(a_path);
		ApplyLogLevel();
		return ok;
	}

	bool Save() { return SaveTo(iniPath); }

	bool SaveTo(const std::string& a_path)
	{
		// The existing file is read back first so comments and ordering survive a save. A file
		// that does not exist yet is not an error - that is a new preset, and WriteKey builds it
		// from nothing.
		std::vector<std::string> lines;
		{
			std::ifstream in(a_path);
			if (in)
			{
				std::string line;
				while (std::getline(in, line)) { lines.push_back(line); }
			}
		}

		bool ok = true;
		ok &= WriteKey(lines, "Debug", "uLogLevel", std::to_string(debug::logLevel));
		ok &= WriteKey(lines, "Levelling", "bOverrideLevelCost", levelling::overrideCost ? "1" : "0");
		ok &= WriteKey(lines, "Levelling", "fLevelUpBase", FormatFloat(levelling::base));
		ok &= WriteKey(lines, "Levelling", "fLevelUpMult", FormatFloat(levelling::mult));

		ok &= WriteKey(lines, "Enchanting", "bOverrideEnchanting", enchanting::overrideCost ? "1" : "0");
		ok &= WriteKey(lines, "Enchanting", "fEnchantingCostBase", FormatFloat(enchanting::costBase));
		ok &= WriteKey(lines, "Enchanting", "fEnchantingCostScale", FormatFloat(enchanting::costScale));
		ok &= WriteKey(lines, "Enchanting", "fEnchantingCostMult", FormatFloat(enchanting::costMult));
		ok &= WriteKey(lines, "Enchanting", "fEnchantingCostExponent", FormatFloat(enchanting::costExponent));

		ok &= WriteKey(lines, "Skills", "bOverrideSkillCaps", skills::overrideCaps ? "1" : "0");
		for (int i = 0; i < skilllist::kCount; ++i)
		{
			ok &= WriteKey(lines, "Skills", SkillKey("fCap", i).c_str(), FormatFloat(skills::cap[i]));
			ok &= WriteKey(lines, "Skills", SkillKey("fFormulaCap", i).c_str(), FormatFloat(skills::formulaCap[i]));
		}

		std::ofstream out(a_path, std::ios::trunc);
		if (!out) { logger::error("Save: could not open {} for writing", a_path); return false; }
		for (const auto& line : lines) { out << line << '\n'; }
		logger::info("settings saved to {}", a_path);
		return ok;
	}

	void RestoreDefaults()
	{
		debug::logLevel = defaults.logLevel;
		levelling::overrideCost = defaults.overrideCost;
		levelling::base = defaults.base;
		levelling::mult = defaults.mult;
		skills::overrideCaps = defaults.overrideCaps;
		for (int i = 0; i < skilllist::kCount; ++i)
		{
			skills::cap[i] = defaults.cap[i];
			skills::formulaCap[i] = defaults.formulaCap[i];
		}
		enchanting::overrideCost = defaults.overrideEnchanting;
		enchanting::costBase = defaults.ench[0];
		enchanting::costScale = defaults.ench[1];
		enchanting::costMult = defaults.ench[2];
		enchanting::costExponent = defaults.ench[3];
		ApplyLogLevel();
	}

	void SetCapturedDefaults(float a_base, float a_mult)
	{
		// "Default" means what this install had before the mod touched it - not a number baked
		// into the DLL from some other copy of the game.
		defaults.base = a_base;
		defaults.mult = a_mult;
	}

	void SetCapturedEnchanting(float a_base, float a_scale, float a_mult, float a_exponent)
	{
		defaults.ench[0] = a_base;
		defaults.ench[1] = a_scale;
		defaults.ench[2] = a_mult;
		defaults.ench[3] = a_exponent;
	}

	void ApplyLogLevel()
	{
		const auto lvl = static_cast<spdlog::level::level_enum>(std::clamp<std::uint32_t>(debug::logLevel, 0u, 6u));
		SKSE::log::set_level(lvl, lvl);
	}

	const std::string& GetIniPath() { return iniPath; }
}
