#include "PCH.h"

#include "Compat.h"

#include "utils/Logger.h"

#include <windows.h>

namespace Compat
{
	namespace
	{
		std::vector<Detection> detections;
		bool altExperience = false;
		bool customSkills = false;
		bool carryElsewhere = false;
		bool ran = false;

		// A loaded SKSE plugin is a module in this process. Asking the loader is the truth;
		// looking for a file on disk is not, because a mod can be installed and disabled.
		bool ModuleLoaded(const char* a_dll)
		{
			return GetModuleHandleA(a_dll) != nullptr;
		}

		// A plugin is present if the game's own data handler loaded it under that filename.
		bool PluginLoaded(const char* a_file)
		{
			auto* handler = RE::TESDataHandler::GetSingleton();
			if (!handler) { return false; }
			return handler->LookupModByName(a_file) != nullptr;
		}

		void Add(std::string a_name, bool a_present, std::string a_consequence)
		{
			detections.push_back({ std::move(a_name), a_present, std::move(a_consequence) });
		}
	}

	void Detect()
	{
		if (ran) { return; }
		ran = true;

		// The filenames were read off the installed mods on this machine rather than guessed.
		const bool experience = ModuleLoaded("Experience.dll");
		const bool uncapper = ModuleLoaded("SkyrimUncapper.dll");
		customSkills = ModuleLoaded("CustomSkills.dll");
		const bool sslr = PluginLoaded("StaticSkillLeveling.esp");
		const bool levelingFreedom = PluginLoaded("Leveling Freedom.esp");
		// Our own carry-weight-per-level mods: both write the player's permanent carry weight from the
		// level, so this mod's carry-weight cross terms would fight them (plan section 16B).
		carryElsewhere = ModuleLoaded("CarryweightOnLevelUp.dll") || ModuleLoaded("CarryWeightPerLevel.dll");

		altExperience = experience;

		Add("Experience", experience,
			experience
				? "it owns where character experience comes from, so the skill-increase-to-level "
				  "multiplier on the Skills tab does nothing while it is installed. Its own page "
				  "says the level COST settings stay fully compatible, so the Levelling tab is "
				  "unaffected and still works."
				: "not installed - the skill-increase-to-level multiplier is ours to set.");

		Add("Custom Skills Framework", customSkills,
			customSkills
				? "installed, so any skills-menu display fix stays OFF by default. The uncapper "
				  "makes this the user's problem - Custom Skills Framework will not load unless "
				  "they edit its INI by hand - and this mod will not do that to anyone: a detected "
				  "conflict switches our own feature off, never someone else's mod."
				: "not installed.");

		Add("Skyrim Skill Uncapper", uncapper,
			uncapper
				? "installed, and it does the same job as this mod's Skills tab. Two mods writing "
				  "the same caps is ambiguous rather than additive - disable one of them."
				: "not installed.");

		Add("Static Skill Leveling Rewritten", sslr,
			sslr
				? "installed. Its own page tells users to install the uncapper as well and hand-edit "
				  "its INI to stop vanilla skill experience. This mod's Static Levelling tab is "
				  "meant to replace that whole arrangement, so run one or the other, not both."
				: "not installed.");

		Add("Carryweight on Level Up / Carry Weight Per Level", carryElsewhere,
			carryElsewhere
				? "installed, and it owns the carry weight a level grants. This mod's carry-weight cross terms "
				  "on the Level Up tab stand down while it is loaded (a level up grants no carry weight from "
				  "here); the attribute gains themselves still apply."
				: "not installed - the carry weight a level grants is set on the Level Up tab.");

		Add("Leveling Freedom", levelingFreedom,
			levelingFreedom
				? "installed, and it sets the same two level-cost values as the Levelling tab. "
				  "Whichever writes last wins, so pick one - this mod only writes them when its "
				  "own override is switched on."
				: "not installed - the Levelling tab has the level cost to itself.");

		for (const auto& d : detections)
		{
			if (d.present) { logger::info("compatibility: {} detected - {}", d.name, d.consequence); }
			else { logger::debug("compatibility: {} not detected", d.name); }
		}
	}

	const std::vector<Detection>& All() { return detections; }
	bool CarryWeightOwnedElsewhere() { return carryElsewhere; }
	bool AlternativeExperienceActive() { return altExperience; }
	bool CustomSkillsFrameworkPresent() { return customSkills; }
}
