#pragma once

// Character Progression Control - the Enchanting tab (the plan's stage 7).
//
// It sits next to the skill caps for a reason: uncapping Enchanting is what breaks the charge
// cost. The cost of using an enchanted item is scaled from the Enchanting skill, so a skill that
// climbs past 100 drives that equation somewhere it was never balanced for, and this tab is the
// dial that puts it back.
//
// Like the Levelling tab it needs NO engine patch - these are the game's own float settings - so
// it is built now while the cap patch waits for a verified address. Same two rules as everywhere
// else in this mod: off by default, and "default" means what this install had.

#include "GameSettings.h"

namespace Enchanting
{
	// Reads this install's own values. Call at kDataLoaded, before Apply.
	void CaptureVanilla();

	void Apply();
	void RequestApply();

	// The group, for the page's live readout and the DevBench tool.
	const GameSettings::Group& Settings();
}
