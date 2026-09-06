#pragma once

// Character Progression Control - stage 5: the health, magicka, stamina and carry weight a level
// up grants, and (1.0.9) the Attributes tab's starting values.
//
// The reference (Kassent's SE uncapper, whose Nexus permissions allow reading it) hooks the entry
// of the game's attribute-level-up routine, reads which attribute the player chose, sets the
// game's own settings for the gain (iAVDhmsLevelUp, fLevelUpCarryWeightMod) to the configured
// values and lets the original code apply them. Same here, with two differences: the settings are
// put back to this install's own values as soon as the original returns, so nothing outside the
// level-up moment sees them changed; and the site is proven before anything is written.
//
// The hook is attached whenever the site is found, because it is also the COUNTER: the game does
// not record how many times each attribute was chosen, so this mod counts every choice itself
// from the moment it is installed on a character (the owner's decision: "we track the attributes
// after install") and keeps the counts in the co-save. Earlier level-ups are unknown, and the
// tab says so rather than inferring them. While "Control what a level up grants" is off, the
// hook hands the game its own values (carry weight on the stamina choice only, exactly vanilla).
//
// Starting values: with the tab's control on, each attribute gets (starting - 100) applied as this
// mod's permanent modifier on top of whatever the character started with - a race that starts
// higher or lower keeps its difference - and the net applied is tracked in the co-save, so turning
// it off takes exactly that away again. The readout's "base" is derived from what this mod knows:
// permanent minus its own share minus what its counted choices granted. No race field is read - the
// race record's starting values (50/50/50 in vanilla) are not what the character starts with.

#include <cstdint>
#include <string>

namespace Attributes
{
	// Registers the patch group. Call at kDataLoaded, before Patches::InstallAll.
	void Register();

	// Testing: the game's own LevelUp Menu choose-attribute function (menu, av), 0 if not resolved.
	std::uintptr_t ChooseAddress();

	// True while the hook is attached and counting choices.
	bool Counting();

	// A save loaded or a new game started (the co-save has been read): starts the count for a
	// character that has none yet, then applies the starting values.
	void OnGameLoaded();

	// Applies the starting-value modifiers for the current settings (main thread only).
	void Apply();
	void RequestApply();

	// SKSE co-save plumbing. Dispatched from Presets::OnLoad by record type.
	inline constexpr std::uint32_t kRecord = 'CPAT';
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void OnRevert();

	struct Row
	{
		float base = 0.0F;           // permanent minus this mod's share and its counted gains: what was there before the count
		float applied = 0.0F;        // this mod's starting-value modifier as applied (co-save)
		float perLevel = 0.0F;       // what a level up grants when this attribute is chosen
		std::uint32_t invested = 0;  // choices counted since the count began
		float gained = 0.0F;         // what those counted choices actually granted (co-save)
		float permanent = 0.0F;      // base + permanent modifiers
		float current = 0.0F;        // incl. temporary effects and damage
	};
	struct State
	{
		std::uint16_t playerLevel = 0;
		std::uint16_t sinceLevel = 0;   // the level the count began at; 1 = the whole history is known
		bool haveHistory = false;       // false until a game has loaded with this mod
		bool controlling = false;
		Row row[3];                     // health, magicka, stamina
	};
	State GetState();
	std::string StatusJson();
}
