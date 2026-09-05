#pragma once

// The level-up menu's labels are "$keys" resolved by the game's Scaleform translator. SKSE fills
// that translator from Interface\Translations\<plugin>_<language>.txt for the plugins it knows;
// measured 2026-09-05, a file named for this DLL was not picked up, so this mod reads its own file
// and adds the entries itself - the same map, the same format (UTF-16LE, "$key<TAB>text" lines).
namespace MenuStrings
{
	// Call at kDataLoaded. Reads Data\Interface\Translations\CharacterProgressionControl_<language>.txt
	// (the game's sLanguage, english when missing) and adds every key the translator lacks.
	void Install();
}
