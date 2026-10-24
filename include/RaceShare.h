#pragma once

// HOW MUCH OF A VALUE THIS MOD OWNS (1.1.10) - pure arithmetic, no game types, so tools\race_share_check.cpp tests it.
//
// The owner, 2026-10-24 01:08: "the carry weight on level up feature of CPC and the mod itself should read what the race's
// base carry weight and stats or skills are, and then apply over the top of it so that it's compatible with race mods";
// and 01:12: "a toggle that decides whether they want our mod to overwrite their race mod".
//
// This mod holds a SHARE of each value, as a permanent modifier, and the co-save remembers the share it holds ("applied").
// Every apply computes the share it should hold now and moves only the difference, so it never reads or writes the race's
// number, and whatever the race, a race mod or another mod adds stays exactly as they set it.
//   additive (default)  carry weight: share = offset + per level x (level - 1), on top of everything else.
//                       starting attribute: share = setting - 100 (100 is the vanilla race start the setting is read
//                       against), on top of the race's own start.
//   our base values     carry weight: the TOTAL permanent carry weight is ours - starting + per level x (level - 1) -
//                       whatever the race and other mods set (the 1.1.9 behaviour). The share is that total minus what
//                       everything else contributes (permanent - applied).
//                       starting attribute: the race's own start is replaced by the setting: share = setting - race start.
//   off                 share = 0 - everything this mod added comes off, and the race's values are back exactly.
// And a second switch (the owner, 01:15: "a toggle that turns off the initial values controls so that just the values per
// level for attributes and carry weight are affected and not the starting values"): with starting values OFF, no starting
// value is touched - the race's and every other mod's starting values stay, an attribute's share is 0 and carry weight's
// share is per level x (level - 1) only, whichever of the two modes above is chosen (with nothing of ours at level 1,
// "our base values" has no starting value to replace).
// Switching any of these at any time moves only this mod's share, so switching back restores the race's values.

#include <algorithm>

namespace raceshare
{
	inline constexpr float kVanillaCarryWeight = 300.0F;  // the race value every 1.1.9 "starting carry weight" was set against
	inline constexpr float kVanillaAttribute = 100.0F;

	struct Carry
	{
		bool on = false;
		bool startingValues = true;  // "Set starting values" - off: per-level growth only
		bool ownBase = false;   // "Use our base values instead of the race's"
		float offset = 0.0F;    // additive: added on top of the race's carry weight at level 1
		float starting = 300.0F;  // our base values: the whole permanent carry weight at level 1
		float perLevel = 5.0F;
		int level = 1;
	};

	inline float Growth(const Carry& a_c) { return a_c.perLevel * static_cast<float>(std::max(0, a_c.level - 1)); }

	// a_permanent: the player's permanent carry weight now (base + permanent modifiers, ours included);
	// a_applied: the share this mod holds now (co-save).
	inline float CarryShare(const Carry& a_c, float a_permanent, float a_applied)
	{
		if (!a_c.on) { return 0.0F; }
		if (!a_c.startingValues) { return Growth(a_c); }
		if (!a_c.ownBase) { return a_c.offset + Growth(a_c); }
		const float everythingElse = a_permanent - a_applied;
		return a_c.starting + Growth(a_c) - everythingElse;
	}

	// a 1.1.9 INI's absolute starting carry weight as this version's offset
	inline float OffsetFromStarting(float a_starting) { return a_starting - kVanillaCarryWeight; }

	inline float AttributeShare(bool a_on, bool a_startingValues, bool a_ownBase, float a_setting, float a_raceStart)
	{
		if (!a_on || !a_startingValues) { return 0.0F; }
		return a_setting - (a_ownBase ? a_raceStart : kVanillaAttribute);
	}
}
