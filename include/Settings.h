#pragma once

// Character Progression Control - settings. Plain-file INI (never the Win32 profile API, so
// PrivateProfileRedirector can neither serve stale values nor overwrite the file).

#include "SkillList.h"

#include <cstdint>
#include <string>
#include <vector>

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

	// Stage 3 - what a use of a skill pays, and what a skill increase pays toward a level.
	// The values are live and saved now; the hooks that consult them are not written yet, which
	// the Patches page states plainly rather than implying otherwise.
	namespace skillexp
	{
		inline bool overrideRates = false;              // bOverrideSkillExp:SkillExperience
		inline float mult[skilllist::kCount]{};         // fMult<Skill>:SkillExperience - 1.0 = vanilla
		inline float toLevelMult = 1.0F;                // fSkillToLevelMult:SkillExperience
	}

	// Stage 4 - what a level up grants.
	namespace levelup
	{
		inline bool overrideRewards = false;            // bOverrideLevelUpRewards:LevelUp
		// Whole perk points per level, as a table by level: from each listed level onward, that many
		// per level up. Vanilla is one row, from level 1, 1 perk. Kept sorted by level; the lookup
		// takes the last row at or below the level reached. There is no fractional perk and no
		// carry-over - a level up grants a whole number.
		struct PerkRow { std::uint16_t fromLevel; std::uint8_t perks; };
		inline std::vector<PerkRow> perksByLevel{ { 1, 1 } };   // sPerksByLevel:LevelUp - "1:1,20:2,40:3"
		int PerksAtLevel(std::uint16_t a_level);
		std::string PerksTableText();                       // "1:1,20:2"
		bool ParsePerksTable(const std::string& a_text);    // replaces the table when the text is valid
		inline float healthPerLevel = 10.0F;            // fHealthPerLevel:LevelUp
		inline float magickaPerLevel = 10.0F;           // fMagickaPerLevel:LevelUp
		inline float staminaPerLevel = 10.0F;           // fStaminaPerLevel:LevelUp
		// The cross terms: carry weight gained when stamina (or magicka, or health) is chosen.
		inline float carryWeightPerHealth = 0.0F;       // fCarryWeightPerHealth:LevelUp
		inline float carryWeightPerMagicka = 0.0F;      // fCarryWeightPerMagicka:LevelUp
		inline float carryWeightPerStamina = 5.0F;      // fCarryWeightPerStamina:LevelUp
	}

	// Stage 6 - a fixed amount per use instead of the vanilla scaling.
	namespace staticlevel
	{
		inline bool enabled = false;                    // bStaticLevelling:StaticLevelling
		inline float xpPerUse[skilllist::kCount]{};     // fPerUse<Skill>:StaticLevelling

		// The skill-point half: skills advance only by points spent in the level-up menu (Static
		// Skill Leveling Rewritten's idea, reproduced here; the menu is its levelupmenu.swf or one of
		// ours). Points per level = per-level + multiplier x level, banked when unspent. Cost tiers by
		// the skill's current level: below 25, 25-49, 50-74, 75 and up.
		inline bool pointsEnabled = false;             // bSkillPoints:StaticLevelling
		inline int pointsPerLevel = 15;                 // iSkillPointsPerLevel:StaticLevelling
		inline float pointsLevelMult = 2.0F;            // fSkillPointsLevelMult:StaticLevelling
		inline int pointsCap = 0;                       // iSkillPointsCap:StaticLevelling (0 = no cap)
		inline int maxIncreasesPerSkill = 5;            // iSkillIncreasesPerLevel:StaticLevelling
		inline int cost[4] = { 3, 5, 7, 9 };            // iSkillCost0/25/50/75:StaticLevelling
	}

	namespace enchanting
	{
		// The charge-cost equation's scaling. Uncapping Enchanting is what breaks this, which is
		// why the two tabs belong together.
		inline bool overrideCost = false;      // bOverrideEnchanting:Enchanting
		inline float costBase = 0.0F;          // fEnchantingCostBase:Enchanting
		inline float costScale = 0.0F;         // fEnchantingCostScale:Enchanting
		inline float costMult = 0.0F;          // fEnchantingCostMult:Enchanting
		inline float costExponent = 0.0F;      // fEnchantingCostExponent:Enchanting
		inline bool seeded = false;            // set once real values are in hand
	}

	// One configuration per game difficulty (Difficulty.cpp): when this is on, the game's
	// difficulty decides which of six presets is in use and changing it switches them.
	namespace difficulty
	{
		inline bool follow = false;   // bFollowDifficulty:Difficulty
	}

	void Init(const std::string& a_iniFileName);
	bool Reload();
	bool Save();

	// The same reader and writer pointed at any file, which is what makes a preset a preset: a
	// preset IS a settings file, in the same format, and nothing special happens to it.
	bool LoadFrom(const std::string& a_path);
	bool SaveTo(const std::string& a_path);
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();

	// Hands the settings layer this install's own level-cost values, so "Restore defaults" goes
	// back to what the game actually had rather than to a compiled-in guess.
	void SetCapturedDefaults(float a_base, float a_mult);
	void SetCapturedEnchanting(float a_base, float a_scale, float a_mult, float a_exponent);
}
