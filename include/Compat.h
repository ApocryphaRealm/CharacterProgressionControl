#pragma once

// Character Progression Control - detecting the mods this one overlaps with.
//
// The plan's section 9 asks for this by name, and the reasoning is worth keeping next to the
// code. The Skyrim Skill Uncapper's own page answers its two known conflicts with "edit the INI
// yourself, before the game will start" - Custom Skills Framework will not even load unless the
// user turns two of the uncapper's settings off by hand. That is the part to beat, and it is
// cheap: detect the mod at load, turn the offending group off ourselves, and SAY SO in plain
// words rather than leaving someone to find a compatibility note.
//
// Three rules this follows, all from that section:
//   * A detected conflict is NEVER a load failure. Another mod failing to load because of our
//     default is exactly the outcome being designed out.
//   * Detection sets the DEFAULT; every switch stays manual, because this detection will
//     eventually be wrong about something.
//   * What was detected, and what it changed, is reported - the log and the Patches tab.
//
// Detection asks the running process rather than inferring from a file path: a loaded DLL is
// checked with GetModuleHandle, and a plugin with the game's own data handler.

#include <string>
#include <vector>

namespace Compat
{
	struct Detection
	{
		std::string name;         // what the player would call it
		bool present = false;
		std::string consequence;  // what its presence means for this mod, in plain words
	};

	// Runs the checks once. Call at kDataLoaded, after the game's plugin list exists.
	void Detect();

	const std::vector<Detection>& All();

	// The findings other code asks about by name.
	bool AlternativeExperienceActive();  // Experience or similar owns the skill-to-level path
	bool CustomSkillsFrameworkPresent();
	bool CarryWeightOwnedElsewhere();    // one of our own carry-weight-per-level mods is loaded
	bool CustomDifficultyUIPresent();    // our standalone Custom Difficulty UI is loaded - the Difficulty tab stands down
	bool BladeAndBluntPresent();         // BladeAndBlunt.esp is loaded - the Difficulty tab's built-in patch applies
	bool RequiemPresent();               // Requiem.esp is loaded - likewise
}
