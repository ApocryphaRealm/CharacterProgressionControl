#pragma once

// Character Progression Control - the patch register.
//
// From the plan's technical shape: "Each patch group installs independently and reports in the
// log whether it installed, so a failed hook degrades to vanilla behaviour rather than to a
// crash." This is that mechanism, and it exists BEFORE the first hook does, on purpose - a patch
// that cannot be installed has to be a reported fact rather than a surprise.
//
// A group that does not install is not an error. It means that part of the mod is off and the
// game behaves exactly as it would without us, and the Patches tab says so in plain words.

#include <functional>
#include <string>
#include <vector>

namespace Patches
{
	struct Group
	{
		std::string name;         // what the player sees
		std::string touches;      // a plain statement of what it changes, for the Patches tab
		bool installed = false;
		std::string status;       // why, when it did not install
	};

	// Installer returns true when the patch is in place; on false it sets a_reason to a plain
	// sentence the player can read.
	using Installer = std::function<bool(std::string& a_reason)>;

	void Register(std::string a_name, std::string a_touches, Installer a_installer);

	// Runs every registered installer once and logs the outcome of each. Call at kDataLoaded.
	void InstallAll();

	const std::vector<Group>& All();

	// True when a named group installed - the guard a feature checks before assuming it works.
	bool IsInstalled(const std::string& a_name);
}
