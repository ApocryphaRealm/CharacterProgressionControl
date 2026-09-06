#pragma once

// Character Progression Control - the Carry Weight tab. Carryweight on Level Up's proven shape,
// absorbed here at the owner's word ("carryweight gets a tab"): the player's PERMANENT carry
// weight (base plus permanent modifiers; enchantments and spells are temporary and left alone)
// is set to  starting + per level x (level - 1)  by applying the difference as this mod's own
// modifier, and that runs exactly when something can have changed: a save loads, the player
// levels up (SKSE LevelIncrease event, and the attribute choice itself), a slider on the tab
// changes, or Apply now is pressed. Nothing polls. The net amount applied so far lives in the
// co-save, so turning the tab off takes exactly that amount away again and asserts nothing.
//
// One owner at a time: while this is on, the Level Up cross terms (carry weight per attribute
// choice) are zeroed, because the formula sets the TOTAL and would undo them at the next
// recalculation. And while one of our standalone carry-weight mods is loaded (Compat), this
// stands down and says so.

#include <cstdint>
#include <string>

namespace CarryWeight
{
	// Registers the level-up event sink. Call once at kDataLoaded.
	void Install();

	// Recomputes and applies the formula for the current level (main thread only).
	void Apply();

	// Queues Apply() onto the main thread (safe from any thread - the UI, DevBench, events).
	void RequestApply();

	// True while this tab owns carry weight (control on and no standalone mod loaded).
	bool Controlling();

	// SKSE co-save plumbing: the net amount applied. Dispatched from Presets::OnLoad by record type.
	inline constexpr std::uint32_t kRecord = 'CPCW';
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
	void OnRevert();

	struct State
	{
		std::uint64_t applications = 0;  // how many times Apply() ran this session
		std::uint16_t playerLevel = 0;
		bool controlling = false;
		std::string standDown;           // why nothing is applied, when a standalone mod owns it
		float applied = 0.0F;            // net amount this mod has added so far (co-save)
		float target = 0.0F;             // what the formula says carry weight should be
		float current = 0.0F;            // current value incl. temporary modifiers
		float permanent = 0.0F;          // base + permanent modifiers - what the formula governs
	};
	State GetState();
	std::string StatusJson();
}
