#pragma once

// Character Progression Control - the Levelling tab's core: what a character level costs.
//
// This is the half of the mod that needs NO engine patch at all. Skyrim computes the experience
// needed for the next level from two game settings:
//
//     cost(level -> level + 1) = fXPLevelUpBase + fXPLevelUpMult x level
//
// so owning the numbers is enough, and it is done by writing the settings and re-writing them
// when something could have changed - a save loading, a new game, or a value changing on the
// page. Nothing runs in the background (design decision: on-demand, never a tick).
//
// Two rules this core follows, both from the plan:
//   * The game's OWN values are captured before anything is written, so "vanilla" is whatever
//     this install actually had rather than a number hard-coded from memory. Restore defaults
//     goes back to those captured values.
//   * We only assert a setting the user has actually turned on. With the override off, the mod
//     writes nothing at all and the game keeps whatever any other mod set. That is the
//     difference between enforcing a setting and sweeping the whole table.

#include <cstdint>

namespace Levelling
{
	// Captures the game's own level-cost settings. Call once at kDataLoaded, before Apply.
	void CaptureVanilla();

	// Writes the current policy into the game settings (main thread only).
	void Apply();

	// Queues Apply() onto the main thread - safe from the UI, DevBench or an event.
	void RequestApply();

	struct State
	{
		bool captured = false;       // whether the game's own values were read yet
		bool overriding = false;     // whether the mod is currently asserting its values
		float vanillaBase = 0.0F;    // the game's own fXPLevelUpBase, as found
		float vanillaMult = 0.0F;    // the game's own fXPLevelUpMult, as found
		float liveBase = 0.0F;       // what the game setting says right now
		float liveMult = 0.0F;       // what the game setting says right now
		std::uint16_t playerLevel = 0;
		float costThisLevel = 0.0F;  // experience needed to reach the next level
		std::uint64_t applications = 0;
	};
	State GetState();

	// Experience needed to go from a_level to a_level + 1 under the given settings.
	float CostForLevel(float a_base, float a_mult, std::uint16_t a_level);
}
