#pragma once

// Character Progression Control - presets (the plan's stage 8), built to the shape decided in
// section 16A rather than the five fixed slots other configuration mods bake into a plugin.
//
//   * A preset IS a file, in the same format as the mod's own INI, in our own folder. Dropping
//     one in makes it selectable; nothing has to be registered and nothing overwrites anything.
//   * The SELECTION is the setting. Whichever preset is chosen is what the mod is using - no
//     inference layer, no hidden merge of one preset over another.
//   * Saving writes the current configuration back INTO the selected preset, so a preset is a
//     living configuration rather than a read-only template.
//   * The selection is per CHARACTER, through the SKSE co-save, while the preset files are
//     shared. Two saves can sit on different presets at once, which is the whole point.
//   * The default is compiled into the DLL, not shipped as a file. It cannot be deleted,
//     corrupted or half-edited, so "restore defaults" always has somewhere true to go and a
//     missing or malformed preset falls back to it rather than to zeros.

#include <string>
#include <vector>

namespace Presets
{
	// The folder presets live in; created on first use.
	std::string Dir();

	// Rescan the folder. Cheap, and done whenever the page opens.
	void Refresh();
	const std::vector<std::string>& All();

	// The compiled-in default, which is what "no preset" means. Never a file.
	inline constexpr const char* kDefaultName = "Default (built in)";

	const std::string& Current();

	// Load a preset and make it the selection. The default name restores the compiled default.
	// A file that cannot be read leaves the settings untouched and returns false.
	bool Select(const std::string& a_name);

	// Write the current configuration into the selected preset. Returns false for the compiled
	// default, which is not a file and is not writable by design.
	bool SaveCurrent();

	// Write the current configuration out as a new preset and select it.
	bool Export(const std::string& a_name);

	bool Rename(const std::string& a_from, const std::string& a_to);
	bool Delete(const std::string& a_name);

	// SKSE co-save: which preset THIS character is on.
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void OnLoad(SKSE::SerializationInterface* a_intfc);
	void OnRevert(SKSE::SerializationInterface* a_intfc);

	// Applies whatever the loaded character's selection turned out to be. Call after a save
	// loads, once the co-save has been read.
	void ApplySelection();
}
