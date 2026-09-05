#pragma once

#include <cstdint>
#include <string>

// Character Progression Control - stage 6, the skill-point half. Skills advance only by points
// spent in the level-up menu: each level grants per-level + multiplier x level points (banked when
// unspent), the menu shows every skill with + / - and the four cost tiers, and the choice is
// applied when the attribute choice is confirmed.
//
// The menu is Static Skill Leveling Rewritten's levelupmenu.swf (its permissions allow it; this
// package ships its vanilla-look variant, and its other skins fit the same contract). The DLL
// does everything the Papyrus side used to: it feeds the movie the caps, the settings and the
// player's skills when the menu opens, listens for the allocation the movie sends back, applies
// each increase through the game's own skill-improve path (so the level-up fires as usual), and
// keeps the bank in the co-save. While points are on, ordinary skill experience is not banked -
// that is the whole point - and a point-spent level pays nothing toward the character level.
namespace SkillPoints
{
	// Registers the patch group and the two event sinks. Call at kDataLoaded, before Patches::InstallAll.
	void Register();

	// Co-save.
	inline constexpr std::uint32_t kRecord = 'SKPT';
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void OnRevert();

	// True while this mod is applying point-bought increases through the skill-improve path: the
	// experience hooks pass the amount through untouched, and the level income is zero.
	bool Applying();

	// State for the page and the test tool.
	int Bank();
	std::uint16_t LastGrantedLevel();
	int PointsForLevel(std::uint16_t a_level);
	const std::string& MenuStatus();   // what the last level-up menu did

	// Testing: bank points; apply an allocation as the menu would ("n0;n1;...;n17", remaining).
	void Grant(int a_points);
	bool ApplyAllocation(const std::string& a_diffs, int a_remaining);
}
