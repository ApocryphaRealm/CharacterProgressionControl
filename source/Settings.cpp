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
		} defaults{};

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

		std::map<std::string, std::string> ReadKeys()
		{
			std::map<std::string, std::string> keys;
			std::ifstream in(iniPath);
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

		bool LoadFileValues()
		{
			if (!std::filesystem::exists(iniPath))
			{
				logger::warn("INI not found at {}; keeping compiled defaults", iniPath);
				return false;
			}
			const auto k = ReadKeys();
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

			// A configuration read from the file counts as seeded, so the capture at kDataLoaded
			// leaves it alone rather than replacing it with the game's own numbers.
			if (sawBase && sawMult) { levelling::seeded = true; }

			logger::info("settings loaded from {}: overrideCost={} base={:.1f} mult={:.1f} logLevel={}",
						 iniPath, levelling::overrideCost, levelling::base, levelling::mult, debug::logLevel);
			return true;
		}

		bool WriteKey(std::vector<std::string>& a_lines, const char* a_section, const char* a_key, const std::string& a_value)
		{
			const std::string wantSection = Lower(a_section);
			const std::string wantKey = Lower(a_key);
			std::string section;
			for (auto& line : a_lines)
			{
				const std::string t = Trim(line);
				if (!t.empty() && t.front() == '[' && t.back() == ']') { section = Lower(t.substr(1, t.size() - 2)); continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos || section != wantSection) { continue; }
				if (Lower(Trim(t.substr(0, eq))) == wantKey)
				{
					line = std::string(a_key) + "=" + a_value;
					return true;
				}
			}
			logger::warn("Save: key {} not found in [{}]", a_key, a_section);
			return false;
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

		defaults = { debug::logLevel, levelling::overrideCost, levelling::base, levelling::mult };

		// Registered for engine-side consistency; NEVER read through the collection - the
		// plain-file parse above is the value of record (redirector-proof).
		auto* collection = utils::INISettingCollection::GetSingleton();
		collection->AddSettings(
			utils::MakeSetting("uLogLevel:Debug", static_cast<unsigned int>(debug::logLevel)),
			utils::MakeSetting("bOverrideLevelCost:Levelling", levelling::overrideCost),
			utils::MakeSetting("fLevelUpBase:Levelling", levelling::base),
			utils::MakeSetting("fLevelUpMult:Levelling", levelling::mult));

		LoadFileValues();
	}

	bool Reload()
	{
		const bool ok = LoadFileValues();
		ApplyLogLevel();
		return ok;
	}

	bool Save()
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(iniPath);
			if (!in) { logger::error("Save: could not open {} for reading", iniPath); return false; }
			std::string line;
			while (std::getline(in, line)) { lines.push_back(line); }
		}

		bool ok = true;
		ok &= WriteKey(lines, "Debug", "uLogLevel", std::to_string(debug::logLevel));
		ok &= WriteKey(lines, "Levelling", "bOverrideLevelCost", levelling::overrideCost ? "1" : "0");
		ok &= WriteKey(lines, "Levelling", "fLevelUpBase", FormatFloat(levelling::base));
		ok &= WriteKey(lines, "Levelling", "fLevelUpMult", FormatFloat(levelling::mult));

		std::ofstream out(iniPath, std::ios::trunc);
		if (!out) { logger::error("Save: could not open {} for writing", iniPath); return false; }
		for (const auto& line : lines) { out << line << '\n'; }
		logger::info("settings saved to {}", iniPath);
		return ok;
	}

	void RestoreDefaults()
	{
		debug::logLevel = defaults.logLevel;
		levelling::overrideCost = defaults.overrideCost;
		levelling::base = defaults.base;
		levelling::mult = defaults.mult;
		ApplyLogLevel();
	}

	void SetCapturedDefaults(float a_base, float a_mult)
	{
		// "Default" means what this install had before the mod touched it - not a number baked
		// into the DLL from some other copy of the game.
		defaults.base = a_base;
		defaults.mult = a_mult;
	}

	void ApplyLogLevel()
	{
		const auto lvl = static_cast<spdlog::level::level_enum>(std::clamp<std::uint32_t>(debug::logLevel, 0u, 6u));
		SKSE::log::set_level(lvl, lvl);
	}

	const std::string& GetIniPath() { return iniPath; }
}
