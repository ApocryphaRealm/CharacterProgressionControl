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
// The half that is real as of 1.0.1: the ADVANCE cap. The game's skill-advance routine loads the
// constant 100.0 at one site and compares the skill against it; Skills.cpp finds that site by its
// byte shape (matched exactly once), proves it by the register it loads and the 100.0 constant,
// and only then redirects the load to CPC_GetSkillCap through a trampoline call. Watched working
// in game 2026-09-05 on SE 1.5.97: a cap of 60 stopped One-handed at exactly 60 through six real
// increments. Opt-in: with Control skill caps off not a byte is written.
//
// Still NOT real: the FORMULA cap - the value the game's own calculations read for a skill - is a
// separate site and is not patched; the Patches tab and the group's status say so plainly.

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
