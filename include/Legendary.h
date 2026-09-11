#pragma once

// Character Progression Control - legendary skills (1.1.5; feature request, Nexus 2026-09-10).
//
// When a skill may be made legendary, the level it drops to (or whether it keeps its level), and
// whether the vanilla Skills menu shows the Legendary hint. Three patch groups, each reported on
// the Patches tab, and none of them adds an assembly stub:
//
//   Legendary reset level - a vtable hook on LegendarySkillResetConfirmCallback::Run. The game makes
//     a skill legendary inside that call, setting its base value from the game setting
//     fLegendarySkillResetValue; the hook writes the chosen level into that setting for the length
//     of the call and puts the game's own value back afterwards. No instruction is rewritten.
//   Legendary threshold / Legendary button - the two places the game compares a skill with 100.0
//     (`comiss xmm0, [rip+100.0]`): whether it may be made legendary, and whether the menu shows the
//     hint. Each is located by shape, proven by the constant it loads, and then only that
//     instruction's 4-byte displacement is pointed at a float of ours. Registers, flags and branches
//     stay the game's own.
//
// Off (the default) = not one byte of the game is written.

#include <string>

namespace Legendary
{
	// Registers the three patch groups and the Skills-menu listener. Call at kDataLoaded, before
	// Patches::InstallAll.
	void Register();

	// Copies the settings into the two values the game's compares read. Runs when the Skills menu
	// opens, so a change on the settings page applies the next time that menu is shown; with the
	// control off both read 100.0, which is vanilla.
	void Refresh();

	// Live state for the cpc.control DevBench tool.
	std::string StatusJson();
}
