#pragma once

// Character Progression Control - a guarded group of game settings.
//
// Several stages of this mod are, underneath, the same job: take a handful of the game's own
// float settings, remember what this installation had, write our values while the player wants
// us to, and put the originals back when they do not. Doing that by hand once per tab is how
// the two rules get quietly broken, so it is done once here.
//
// The rules it enforces:
//   * Capture BEFORE writing. "Vanilla" is what this install actually had, never a number
//     compiled in from memory.
//   * A setting that does not exist on this runtime is logged and skipped, not guessed at. The
//     group reports which of its settings were found, so a tab can say so instead of pretending.
//   * With the policy off, the captured values are restored and nothing else is touched.

#include <string>
#include <vector>

namespace GameSettings
{
	struct Entry
	{
		const char* name = nullptr;  // the game setting, e.g. "fEnchantingSkillCostBase"
		float* value = nullptr;      // our configured value, owned by the settings layer
		float vanilla = 0.0F;        // what this install had
		bool found = false;          // whether the runtime has this setting at all
	};

	class Group
	{
	public:
		explicit Group(std::string a_label) : label(std::move(a_label)) {}

		// Declare a setting the group owns. Call before Capture.
		void Add(const char* a_name, float* a_value);

		// Read this install's own values. Safe to call more than once; only the first counts.
		void Capture();

		// Write our values when a_active, else restore what was captured.
		void Apply(bool a_active);

		bool Captured() const { return captured; }
		int FoundCount() const;
		const std::vector<Entry>& Entries() const { return entries; }

		// The value the game is using right now, for a live readout.
		float Live(int a_index) const;

		// Seed our configured values from this install's own, for a first run.
		void SeedFromVanilla();

	private:
		std::string label;
		std::vector<Entry> entries;
		bool captured = false;
	};
}
