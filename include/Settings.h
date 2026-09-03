#pragma once

// Character Progression Control - settings. Plain-file INI (never the Win32 profile API, so
// PrivateProfileRedirector can neither serve stale values nor overwrite the file).

#include "SkillList.h"

#include <cstdint>
#include <string>

namespace settings
{
	namespace debug
	{
		inline std::uint32_t logLevel = 0;  // uLogLevel:Debug - 0 = trace (project default)
	}

	namespace levelling
	{
		// What a character level costs:  fXPLevelUpBase + fXPLevelUpMult x level.
		//
		// The override is OFF by default and that is deliberate: installing this mod must not
		// silently change anyone's levelling. With it off the mod restores this install's own
		// values and writes nothing else.
		inline bool overrideCost = false;  // bOverrideLevelCost:Levelling
		inline float base = 75.0F;         // fLevelUpBase:Levelling - seeded from the game on first run
		inline float mult = 25.0F;         // fLevelUpMult:Levelling - seeded from the game on first run

		// True once base/mult hold real values - either read from the INI or captured from the
		// game itself. Stops the capture from overwriting a configuration the player saved.
		inline bool seeded = false;
	}

	namespace skills
	{
		// Same rule as the level cost: off by default, and while it is off nothing is asserted.
		inline bool overrideCaps = false;                  // bOverrideSkillCaps:Skills

		// Where a skill stops advancing, and separately the value the game's own formulas read
		// for it - so a skill can show 300 while combat maths still treats it as 100. Both are
		// filled with 100 at startup, which is vanilla.
		inline float cap[skilllist::kCount]{};             // fCap<Skill>:Skills
		inline float formulaCap[skilllist::kCount]{};      // fFormulaCap<Skill>:Skills
	}

	void Init(const std::string& a_iniFileName);
	bool Reload();
	bool Save();
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();

	// Hands the settings layer this install's own level-cost values, so "Restore defaults" goes
	// back to what the game actually had rather than to a compiled-in guess.
	void SetCapturedDefaults(float a_base, float a_mult);
}
