#include "PCH.h"

#include "Presets.h"
#include "Attributes.h"
#include "CarryWeight.h"
#include "DifficultyValues.h"
#include "ExperienceSources.h"
#include "SkillPoints.h"

#include "Enchanting.h"
#include "Levelling.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <filesystem>

namespace Presets
{
	namespace
	{
		constexpr std::uint32_t kRecord = 'PRST';
		constexpr std::uint32_t kVersion = 1;

		std::vector<std::string> names;
		std::string current = kDefaultName;

		std::string PathFor(const std::string& a_name)
		{
			return (std::filesystem::path(Dir()) / (a_name + ".ini")).string();
		}

		bool IsDefault(const std::string& a_name) { return a_name == kDefaultName; }

		// A preset name becomes a file name, so it has to be one. Rejected rather than mangled -
		// silently renaming what the player typed is how a preset goes missing.
		bool NameIsUsable(const std::string& a_name)
		{
			if (a_name.empty() || a_name.size() > 64) { return false; }
			if (IsDefault(a_name)) { return false; }
			for (const char c : a_name)
			{
				if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
					c == '"' || c == '<' || c == '>' || c == '|') { return false; }
			}
			return true;
		}

		void ReapplyEverything()
		{
			Levelling::RequestApply();
			Enchanting::RequestApply();
			Attributes::RequestApply();
			CarryWeight::RequestApply();
			DifficultyValues::RequestApply();
		}
	}

	std::string Dir()
	{
		const auto dir = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
						 "CharacterProgressionControl" / "Presets";
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		return dir.string();
	}

	void Refresh()
	{
		names.clear();
		names.push_back(kDefaultName);   // always first, always present

		std::error_code ec;
		const auto dir = std::filesystem::path(Dir());
		if (!std::filesystem::exists(dir, ec)) { return; }

		for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
		{
			if (ec) { break; }
			if (!entry.is_regular_file(ec)) { continue; }
			const auto& p = entry.path();
			if (p.extension() != ".ini") { continue; }
			names.push_back(p.stem().string());
		}
		logger::debug("presets: {} file(s) plus the built-in default", names.size() - 1);
	}

	const std::vector<std::string>& All() { return names; }

	const std::string& Current() { return current; }

	bool Select(const std::string& a_name)
	{
		if (IsDefault(a_name))
		{
			// The built-in default means the mod's own INI - the base configuration a player
			// edits when they are not using presets at all. If that file is missing or will not
			// parse, the compiled defaults catch it, which is the whole point of having a default
			// that is code rather than a file: there is always somewhere true to land.
			if (!settings::Reload())
			{
				logger::warn("the mod's INI could not be read; falling back to the compiled defaults");
				settings::RestoreDefaults();
			}
			current = kDefaultName;
			ReapplyEverything();
			logger::info("preset: using the built-in default");
			return true;
		}

		const auto path = PathFor(a_name);
		if (!std::filesystem::exists(path))
		{
			// The file is gone - fall back to the default rather than to whatever was in memory,
			// which is the failure other configuration mods are known for.
			logger::warn("preset \"{}\" is missing; falling back to the built-in default", a_name);
			settings::RestoreDefaults();
			current = kDefaultName;
			ReapplyEverything();
			return false;
		}

		if (!settings::LoadFrom(path))
		{
			logger::warn("preset \"{}\" could not be read; falling back to the built-in default", a_name);
			settings::RestoreDefaults();
			current = kDefaultName;
			ReapplyEverything();
			return false;
		}

		current = a_name;
		ReapplyEverything();
		logger::info("preset: using \"{}\"", a_name);
		return true;
	}

	bool SaveCurrent()
	{
		if (IsDefault(current))
		{
			logger::info("the built-in default is not a file and cannot be written to; use Export "
						 "to make a preset from the current settings");
			return false;
		}
		return settings::SaveTo(PathFor(current));
	}

	bool Export(const std::string& a_name)
	{
		if (!NameIsUsable(a_name))
		{
			logger::warn("\"{}\" is not a usable preset name", a_name);
			return false;
		}
		if (!settings::SaveTo(PathFor(a_name))) { return false; }
		Refresh();
		current = a_name;
		logger::info("preset \"{}\" written and selected", a_name);
		return true;
	}

	bool Rename(const std::string& a_from, const std::string& a_to)
	{
		if (IsDefault(a_from) || !NameIsUsable(a_to)) { return false; }
		std::error_code ec;
		std::filesystem::rename(PathFor(a_from), PathFor(a_to), ec);
		if (ec)
		{
			logger::warn("could not rename preset \"{}\": {}", a_from, ec.message());
			return false;
		}
		if (current == a_from) { current = a_to; }
		Refresh();
		return true;
	}

	bool Delete(const std::string& a_name)
	{
		if (IsDefault(a_name)) { return false; }
		std::error_code ec;
		std::filesystem::remove(PathFor(a_name), ec);
		if (ec)
		{
			logger::warn("could not delete preset \"{}\": {}", a_name, ec.message());
			return false;
		}
		if (current == a_name) { Select(kDefaultName); }
		Refresh();
		return true;
	}

	// ---- per-character selection ---------------------------------------------------------

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc->OpenRecord(kRecord, kVersion)) { return; }
		const std::uint32_t length = static_cast<std::uint32_t>(current.size());
		a_intfc->WriteRecordData(&length, sizeof(length));
		if (length) { a_intfc->WriteRecordData(current.data(), length); }
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		std::uint32_t type = 0, version = 0, length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length))
		{
			if (type == SkillPoints::kRecord) { SkillPoints::ReadRecord(a_intfc, version, length); continue; }
			if (type == Attributes::kRecord) { Attributes::ReadRecord(a_intfc, version, length); continue; }
			if (type == CarryWeight::kRecord) { CarryWeight::ReadRecord(a_intfc, version, length); continue; }
			if (type == ExperienceSources::kRecord) { ExperienceSources::ReadRecord(a_intfc, version, length); continue; }
			if (type != kRecord) { continue; }
			std::uint32_t nameLength = 0;
			if (!a_intfc->ReadRecordData(&nameLength, sizeof(nameLength))) { continue; }
			if (nameLength == 0 || nameLength > 256) { current = kDefaultName; continue; }
			std::string name(nameLength, '\0');
			if (!a_intfc->ReadRecordData(name.data(), nameLength)) { current = kDefaultName; continue; }
			current = name;
			logger::info("this character is on preset \"{}\"", current);
		}
	}

	void OnRevert(SKSE::SerializationInterface*)
	{
		// Between characters the selection goes back to the default, so a new game never inherits
		// the previous character's preset.
		current = kDefaultName;
	}

	void ApplySelection()
	{
		Refresh();
		Select(current);
	}
}
