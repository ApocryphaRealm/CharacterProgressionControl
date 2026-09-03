#pragma once

// Character Progression Control - the Skills tab.
//
// Two halves, and they are at very different stages on purpose.
//
// The half that is REAL now: reading. The game keeps the player's progression in
// PlayerCharacter::skills - every skill's level, its experience and its threshold, plus the
// CHARACTER's own experience and threshold. All of that is reachable through CommonLibSSE-NG
// with no engine patch and no address of our own, so the tab has the live readout the plan
// requires from the day it ships, and the character-level numbers double as a check on the
// Levelling tab: the cost this mod computes can be compared against the threshold the game is
// actually holding.
//
// The half that is NOT real yet: raising the caps. Letting a skill advance past 100, and
// separately controlling the value the game's own formulas use, both need engine hooks, and an
// engine hook needs a verified Address Library ID. Inventing one crashes somebody's game, so the
// cap patch is registered as a patch group whose installer reports honestly that it is not
// implemented. The settings below are stored and shown, and they do nothing until that group
// installs - which the Patches tab states plainly rather than implying otherwise.

#include "SkillList.h"

#include <cstdint>

namespace Skills
{
	// Registers the cap patch group. Call at kDataLoaded, before Patches::InstallAll.
	void Register();

	struct SkillState
	{
		float level = 0.0F;
		float xp = 0.0F;
		float levelThreshold = 0.0F;
	};

	struct State
	{
		bool readable = false;   // whether the player's skill data could be read at all
		std::uint16_t characterLevel = 0;
		float characterXp = 0.0F;
		float characterThreshold = 0.0F;
		SkillState skill[skilllist::kCount];
	};
	State GetState();
}
