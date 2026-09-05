#pragma once

// Character Progression Control - one configuration per game difficulty (the owner, 2026-09-05:
// "make cpc listen to the games difficulty setting and save the current settings for the active
// difficulty ... a listener and separate ini files, 1 per difficulty level that auto switches").
//
// Built on the presets the mod already has, because a per-difficulty configuration IS a preset:
// a file in the Presets folder, in the INI's own format. Six of them, named after the game's
// six difficulty levels (Novice, Apprentice, Adept, Expert, Master, Legendary - Skyrim SE has six,
// not five). When "follow the game's difficulty" is on, the difficulty the game reports decides
// which of the six is in use, and changing difficulty in the game's own Settings menu switches:
// the configuration in hand is written back into the OLD difficulty's file and the NEW
// difficulty's file is loaded (created from the current configuration the first time it is
// met, so a fresh difficulty never starts from zeros).
//
// The "listener" is event-driven, not a tick: the difficulty is read when a save loads and when
// the Journal Menu closes (the game's Settings live under its System tab), which are the only
// moments it can change. Nothing polls.

#include <cstdint>
#include <string>

namespace Difficulty
{
	inline constexpr int kCount = 6;

	// The game's own index (PlayerCharacter difficulty: 0 Novice .. 5 Legendary), or -1 when no
	// player exists yet (main menu).
	int Current();
	const char* NameOf(int a_difficulty);
	inline const char* CurrentName() { return NameOf(Current()); }

	// "Difficulty - Adept" - the preset file that difficulty maps to.
	std::string PresetNameFor(int a_difficulty);

	// Registers the menu listener. Call once at kDataLoaded.
	void Install();

	// Compares the game's difficulty with the last one applied and switches presets when they
	// differ (and following is on). a_reason is logged. Safe to call any time on the main thread.
	void Sync(const char* a_reason);

	// The settings page's toggle changed: when it turned on, adopt the current difficulty at
	// once; when it turned off, leave the configuration where it is. Returns a status line.
	std::string OnFollowChanged();

	// For the DevBench tool: set the game's difficulty (0..5) as the Settings menu would, then
	// Sync. Returns false for an index outside the range or no player.
	bool SetGameDifficulty(int a_difficulty);

	// JSON for the DevBench tool.
	std::string StatusJson();
}
