#pragma once

// Character Progression Control - stage 5: the health, magicka, stamina and carry weight a level
// up grants.
//
// The reference (Kassent's SE uncapper, whose Nexus permissions allow reading it) hooks the entry
// of the game's attribute-level-up routine, reads which attribute the player chose, sets the
// game's own settings for the gain (iAVDhmsLevelUp, fLevelUpCarryWeightMod) to the configured
// values and lets the original code apply them. Same here, with two differences: the settings are
// put back to this install's own values as soon as the original returns, so nothing outside the
// level-up moment sees them changed; and the site is proven before anything is written.

namespace Attributes
{
	// Registers the patch group. Call at kDataLoaded, before Patches::InstallAll.
	void Register();
}
