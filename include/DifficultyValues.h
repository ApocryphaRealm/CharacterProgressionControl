#pragma once

// Character Progression Control - the Difficulty tab (1.1.0, extended 1.1.2). Custom Difficulty
// UI's mechanic inside this mod, at the owner's word ("build cd ui into cpc"; Custom Difficulty UI
// itself stays a separate minimal mod). Two halves, both plain GameSetting writes with no engine hook:
//
//   * DAMAGE - the twelve vanilla multipliers, one pair per difficulty: fDiffMultHPToPC*
//     (damage dealt TO the player) and fDiffMultHPByPC* (damage dealt BY the player). The game
//     reads the pair for the difficulty being played, so every pair is written and the game's
//     own difficulty setting picks. With "one pair for every difficulty" on, the same pair is
//     written six times (Yet Another Difficulty Mod's Simple mode).
//   * REGENERATION - vanilla has ONE value for every difficulty (fCombatHealthRegenRateMult and
//     friends). This tab keeps one value per difficulty for each of seven settings and writes
//     the one for the CURRENT difficulty into the game's single setting, re-writing when the
//     difficulty changes (the Journal Menu closing, the same moment the preset follower watches).
//     Five more are global by design (the delay ceilings and two situational values).
//
// OFF WRITES NOTHING (1.1.2, logic library 43). Every value is captured at kDataLoaded - after
// every plugin's records - as the LOADED value: vanilla on a plain game, Blade and Blunt's or
// Requiem's numbers when they are installed. A control that is off never writes; an on->off flip
// puts the loaded values back once. The compiled vanilla numbers are the slider defaults only.
//
// DIFFICULTY BY LEVEL (1.1.2, Yet Another Difficulty Mod's Dynamic mode): six level thresholds,
// one per difficulty; on a save load and on every level-up the highest difficulty whose threshold
// the player has reached becomes the game's difficulty, through the same path the Settings menu
// uses. Blade and Blunt's level-based scaling is the same idea one tier per ten levels, so its
// milestones are the defaults.
//
// THE BUILT-IN PATCH (1.1.2): BladeAndBlunt.esp and Requiem.esp are detected; their values are the
// loaded values, three presets fill the table (loaded / Blade and Blunt / Requiem), and while a
// control is on this tab writes last - through the SKSE task queue after every synchronous handler
// of the same event, and again on the LevelIncrease event, the moment Blade and Blunt's DLL steps
// its multipliers - so this mod supersedes theirs.
//
// One owner at a time: while CustomDifficultyUI.dll is loaded, this tab stands down and says so.

#include <string>

namespace DifficultyValues
{
	inline constexpr int kDifficulties = 6;
	inline constexpr int kRegenPerDifficulty = 7;
	inline constexpr int kRegenGlobal = 5;

	// Resolves every GameSetting, captures the loaded values, seeds unset slots, installs the
	// level-up listener and applies once. Call at kDataLoaded after Compat::Detect().
	void Init();

	// Writes every setting for the current controls and the current difficulty (main thread).
	void Apply();
	void RequestApply();

	// A save loaded or a new game started: the level rule runs, then everything is re-applied
	// from the task queue (so it lands after every other plugin's own load handler).
	void OnGameLoaded();

	// The Journal Menu closed: if the game's difficulty moved, the regeneration set for the new
	// difficulty is written. Called from Difficulty's own menu listener.
	void OnMenuClosed();

	// The difficulty-by-level rule: sets the game's difficulty from the player's level when the
	// rule is on and the level says a different tier. Returns the difficulty it settled on, or -1.
	int ApplyLevelRule(const char* a_reason);
	int LevelRuleTarget(int a_level);   // what the table says for a level, -1 when no row applies

	bool StandingDown();   // a control is on but Custom Difficulty UI owns the values
	int LastAppliedDifficulty();

	// Names, values and live readings for the page and the DevBench tool.
	const char* DamageSettingName(int a_difficulty, bool a_toPlayer);
	const char* RegenSettingName(int a_index);
	const char* GlobalSettingName(int a_index);
	bool RegenResolved(int a_index);
	bool GlobalResolved(int a_index);
	float RegenVanilla(int a_index);    // the loaded value (what this game came with)
	float GlobalVanilla(int a_index);
	float LoadedDamage(int a_difficulty, bool a_toPlayer);   // captured at kDataLoaded
	float LiveDamage(int a_difficulty, bool a_toPlayer);
	float LiveRegen(int a_index);
	float LiveGlobal(int a_index);

	// The presets that fill the per-difficulty damage table.
	void UseLoadedValues();     // whatever this game loaded with - any overhaul's numbers
	void UseVanillaValues();
	void UseBladeAndBlunt();    // its ESP's records, read 2026-09-06 (open permissions)
	void UseRequiem();          // all 1.0 - its repository's records

	// The overhauls the built-in patch knows. BladeAndBluntLevelScaling: its INI's
	// bLevelBasedDifficulty as read at Init (a_found false when the file or key is absent).
	std::string OverhaulLoaded();   // "" when none
	bool BladeAndBluntLevelScaling(bool& a_found);

	// Every difficulty's regeneration set becomes a copy of a_from's.
	void CopyRegenToAll(int a_from);

	std::string StatusJson();
}
