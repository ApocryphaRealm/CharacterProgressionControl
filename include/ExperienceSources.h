#pragma once

// Character Progression Control - stage 9: alternative experience sources (the owner, 2026-09-05:
// "do stage 9"). Character experience from what the character DOES rather than from skill use:
// quests completed (by quest type), locations discovered, dungeons cleared, kills, books read.
// Written from scratch on the game's own story events (QuestStatus, LocationDiscovery,
// LocationCleared, ActorKill, BooksRead); nothing here consults or resembles any other mod's
// code - the one released mod in this space has a closed licence (plan section 1).
//
// The amount for a source goes straight onto the character's experience (PlayerSkills::data->xp,
// the number the level-up threshold is compared against), and when that crosses the threshold the
// game's own "level up available" notice is shown. Skill increases keep paying toward the level
// (supplement) or stop paying (replace) - the latter through the level-income hook this mod already
// owns, which returns 0 while replace mode is on.
//
// One owner at a time: while the Experience mod (Experience.dll) is loaded, this stands down.

#include <cstdint>
#include <string>

namespace ExperienceSources
{
	enum Source : int { kQuest = 0, kLocation, kCleared, kKill, kBook, kCount };

	// Registers the five event sinks. Call once at kDataLoaded.
	void Install();

	// True while the tab owns experience (enabled and the Experience mod is not loaded).
	bool Controlling();
	bool StandingDown();

	// Grants a_amount of character experience for a_source (main thread). Used by the sinks and
	// by the DevBench simulation op.
	void Grant(Source a_source, float a_amount, const std::string& a_what);

	// SKSE co-save: what this character has earned from each source since the count began.
	inline constexpr std::uint32_t kRecord = 'CPXS';
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void OnRevert();

	struct State
	{
		std::uint32_t count[kCount]{};
		float xp[kCount]{};
		std::string last;   // "Quest completed: Bleak Falls Barrow (+300)"
	};
	State GetState();
	std::string StatusJson();
}
