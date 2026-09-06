#pragma once

// Character Progression Control - the Difficulty tab (1.1.0). Custom Difficulty UI's mechanic
// inside this mod, at the owner's word ("build cd ui into cpc"; Custom Difficulty UI itself stays
// a separate minimal mod). Two halves, both plain GameSetting writes with no engine hook:
//
//   * DAMAGE - the twelve vanilla multipliers, one pair per difficulty: fDiffMultHPToPC*
//     (damage dealt TO the player) and fDiffMultHPByPC* (damage dealt BY the player). The game
//     reads the pair for the difficulty being played, so every pair is written and the game's
//     own difficulty setting picks. Off = the real vanilla numbers restored, not a freeze.
//   * REGENERATION - vanilla has ONE value for every difficulty (fCombatHealthRegenRateMult and
//     friends). This tab keeps one value per difficulty for each of seven settings and writes
//     the one for the CURRENT difficulty into the game's single setting, re-writing when the
//     difficulty changes (the Journal Menu closing, the same moment the preset follower watches).
//     Five more are global by design (the delay ceilings and two situational values). Their
//     vanilla values are captured from the running game at first sight, so "off" restores what
//     this install came with.
//
// One owner at a time: while CustomDifficultyUI.dll is loaded, this tab stands down and says so.

#include <string>

namespace DifficultyValues
{
	inline constexpr int kDifficulties = 6;
	inline constexpr int kRegenPerDifficulty = 7;
	inline constexpr int kRegenGlobal = 5;

	// Resolves every GameSetting, captures the regeneration vanilla values, seeds unset slots
	// and applies once. Call at kDataLoaded after Compat::Detect().
	void Init();

	// Writes every setting for the current controls and the current difficulty (main thread).
	void Apply();
	void RequestApply();

	// The Journal Menu closed: if the game's difficulty moved, the regeneration set for the new
	// difficulty is written. Called from Difficulty's own menu listener.
	void OnMenuClosed();

	bool StandingDown();   // a control is on but Custom Difficulty UI owns the values
	int LastAppliedDifficulty();

	// Names, vanilla values and live readings for the page and the DevBench tool.
	const char* DamageSettingName(int a_difficulty, bool a_toPlayer);
	const char* RegenSettingName(int a_index);
	const char* GlobalSettingName(int a_index);
	bool RegenResolved(int a_index);
	bool GlobalResolved(int a_index);
	float RegenVanilla(int a_index);
	float GlobalVanilla(int a_index);
	float LiveDamage(int a_difficulty, bool a_toPlayer);
	float LiveRegen(int a_index);
	float LiveGlobal(int a_index);

	// Every difficulty's regeneration set becomes a copy of a_from's.
	void CopyRegenToAll(int a_from);

	std::string StatusJson();
}
