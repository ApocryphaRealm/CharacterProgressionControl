#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Levelling.h"
#include "Patches.h"
#include "SkillList.h"
#include "Skills.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <functional>
#include <string>

namespace UI
{
	namespace
	{
		std::string statusMessage;
		std::string selectedSlider;

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface())
			{
				taskInterface->AddTask(std::move(a_task));
			}
		}

		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igSliderFloat",
				"igCheckbox",
				"igIsKeyPressed_Bool",
				"igIsItemClicked",
				"igIsItemActive",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igPushItemWidth",
				"igPopItemWidth",
				"igPushID_Str",
				"igPopID"
			};

			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("The menu framework does not export \"{}\"", name);
					return false;
				}
			}
			return true;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("(?)");
			if (ImGuiMCP::IsItemHovered())
			{
				ImGuiMCP::SetTooltip("%s", a_description);
			}
		}

		bool NudgeableSlider(const char* a_label, float* a_value, float a_min, float a_max,
							 const char* a_format, float a_step)
		{
			bool changed = ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);
			if (ImGuiMCP::IsItemClicked() || ImGuiMCP::IsItemActive()) { selectedSlider = a_label; }
			if (selectedSlider == a_label)
			{
				float nudge = 0.0F;
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_LeftArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow)) { nudge -= a_step; }
				if (ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_RightArrow) || ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow)) { nudge += a_step; }
				if (nudge != 0.0F)
				{
					*a_value = std::clamp(*a_value + nudge, a_min, a_max);
					changed = true;
				}
				ImGuiMCP::SameLine();
				ImGuiMCP::TextDisabled("<-->");
			}
			return changed;
		}

		// Every tab carries the same three actions, so the page a player happens to be on is
		// never the wrong place to save.
		void RenderButtons()
		{
			ImGuiMCP::SeparatorText("");

			if (ImGuiMCP::Button("Save"))
			{
				statusMessage = "Saving...";
				OnMainThread([]() {
					statusMessage = settings::Save() ? "Settings saved." : "Could not write the INI. See the log for why.";
				});
			}
			HelpMarker("Writes every setting on these pages to the plugin's INI so it survives a restart.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Reload from INI"))
			{
				statusMessage = "Reloading...";
				OnMainThread([]() {
					const bool ok = settings::Reload();
					Levelling::Apply();
					statusMessage = ok ? "Settings reloaded from the INI."
									   : "Could not read the INI. See the log for why.";
				});
			}
			HelpMarker("Throws away any change made here since the last save and re-reads the INI from disk.");

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button("Restore defaults"))
			{
				OnMainThread([]() {
					settings::RestoreDefaults();
					Levelling::Apply();
					logger::debug("Restored default settings");
				});
				statusMessage = "Defaults restored - the values this install had before the mod. Press Save to keep them.";
			}
			HelpMarker("Puts every setting back to the value this installation had before the mod touched it. Nothing is written until you press Save.");

			if (!statusMessage.empty())
			{
				ImGuiMCP::TextWrapped("%s", statusMessage.c_str());
			}

			ImGuiMCP::Spacing();
			ImGuiMCP::Text("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("No menu framework is installed; settings will be read from the INI only");
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("The installed menu framework is older than this plugin's settings "
						 "menu needs. Update it (Apocrypha Menu Framework, or SKSE Menu "
						 "Framework version 3 or newer).");
			return;
		}

		SKSEMenuFramework::SetSection("Character Progression Control");
		SKSEMenuFramework::AddSectionItem("Levelling", LevellingPanel::Render);
		SKSEMenuFramework::AddSectionItem("Skills", SkillsPanel::Render);
		SKSEMenuFramework::AddSectionItem("Patches", PatchesPanel::Render);
		SKSEMenuFramework::AddSectionItem("Debug", DebugPanel::Render);
		logger::info("Registered the settings pages with the menu framework");
	}

	void __stdcall LevellingPanel::Render()
	{
		using namespace settings;

		const auto s = Levelling::GetState();

		ImGuiMCP::TextWrapped("What one character level costs. Skyrim works out the experience needed for "
							  "your next level as:  base + (per-level x your level).");
		ImGuiMCP::Spacing();

		if (!s.captured)
		{
			ImGuiMCP::TextWrapped("This install's own level-cost settings could not be read, so this tab will "
								  "not write anything. See the log.");
			RenderButtons();
			return;
		}

		ImGuiMCP::PushItemWidth(260.0F);

		ImGuiMCP::SeparatorText("Cost of a level");

		bool changed = false;
		bool over = levelling::overrideCost;
		if (ImGuiMCP::Checkbox("Control the cost of a level", &over))
		{
			levelling::overrideCost = over;
			changed = true;
		}
		HelpMarker("Off by default, and while it is off this mod writes nothing - your game levels exactly "
				   "as it did before, and any other mod that sets these values keeps them. Turn it on and the "
				   "two values below are applied and re-applied every time a save loads.");

		if (levelling::overrideCost)
		{
			changed |= NudgeableSlider("Base cost", &levelling::base, 0.0F, 2000.0F, "%.0f", 5.0F);
			HelpMarker("The flat part of the cost, paid at every level. This install's own value is shown below.");

			changed |= NudgeableSlider("Per level", &levelling::mult, 0.0F, 500.0F, "%.1f", 1.0F);
			HelpMarker("Added to the cost for each level you already have. Raising this makes later levels "
					   "progressively slower; lowering it flattens the curve.");
		}

		if (changed) { Levelling::RequestApply(); }

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("What that means right now");

		// The readout is what proves the setting took - one of the plan's two rules for a tab.
		ImGuiMCP::Text("Level %u to %u costs %.0f experience  (%.0f + %.1f x %u)",
					   s.playerLevel, s.playerLevel + 1, s.costThisLevel,
					   s.liveBase, s.liveMult, s.playerLevel);
		ImGuiMCP::Text("This install's own values: %.0f base, %.1f per level", s.vanillaBase, s.vanillaMult);
		if (!s.overriding)
		{
			ImGuiMCP::TextDisabled("Not controlling the cost - the values above are whatever the game is using.");
		}

		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button("Apply now"))
		{
			Levelling::RequestApply();
			statusMessage = "Applied.";
		}
		HelpMarker("Re-writes the values. It also runs by itself when a save loads, when a new game starts, "
				   "and when you change anything above - nothing runs in the background.");

		RenderButtons();
	}

	void __stdcall SkillsPanel::Render()
	{
		using namespace settings;

		const auto s = Skills::GetState();
		const bool capsActive = Patches::IsInstalled("Skill caps");

		ImGuiMCP::TextWrapped("Where each skill stops, and what the game's own formulas read for it - "
							  "so a skill can show 300 while combat maths still treats it as 100.");
		ImGuiMCP::Spacing();

		if (!capsActive)
		{
			// Said plainly rather than left to be discovered. The values below are still stored
			// and saved, so a configuration made now is ready the day the patch lands.
			ImGuiMCP::TextWrapped("The skill cap patch is NOT active in this build, so every skill still "
								  "stops at 100 exactly as in vanilla. The values below are remembered "
								  "and saved, but they do nothing yet. See the Patches tab.");
			ImGuiMCP::Spacing();
		}

		ImGuiMCP::PushItemWidth(200.0F);

		bool over = skills::overrideCaps;
		if (ImGuiMCP::Checkbox("Control skill caps", &over)) { skills::overrideCaps = over; }
		HelpMarker("Off by default. While it is off this mod asserts nothing about your skills.");

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Your skills right now");

		if (!s.readable)
		{
			ImGuiMCP::TextDisabled("No character loaded - load a save to see your skills.");
			ImGuiMCP::PopItemWidth();
			RenderButtons();
			return;
		}

		// The live readout: the game's own numbers, straight out of the player's progression
		// data. It is also the check on the Levelling tab - the character threshold below is
		// what the game is really holding.
		ImGuiMCP::Text("Character level %u - %.0f of %.0f experience toward the next level",
					   s.characterLevel, s.characterXp, s.characterThreshold);
		ImGuiMCP::Spacing();

		for (int i = 0; i < skilllist::kCount; ++i)
		{
			ImGuiMCP::PushID(skilllist::kIniName[i]);

			ImGuiMCP::Text("%-12s  %5.1f   %.0f / %.0f", skilllist::kDisplayName[i],
						   s.skill[i].level, s.skill[i].xp, s.skill[i].levelThreshold);

			if (skills::overrideCaps)
			{
				NudgeableSlider("Cap", &skills::cap[i], 100.0F, 1000.0F, "%.0f", 5.0F);
				HelpMarker("The level this skill stops advancing at. 100 is vanilla.");

				NudgeableSlider("Formula cap", &skills::formulaCap[i], 10.0F, 1000.0F, "%.0f", 5.0F);
				HelpMarker("The value the game's own calculations use for this skill, however high "
						   "the skill itself reads. Leaving this at 100 keeps combat and prices "
						   "balanced while the skill number keeps climbing.");
			}

			ImGuiMCP::PopID();
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	void __stdcall PatchesPanel::Render()
	{
		ImGuiMCP::TextWrapped("Each engine patch this mod installs, and what it changes. A patch that is "
							  "not active is not a fault - that part of the game simply behaves as it "
							  "would without this mod.");
		ImGuiMCP::Spacing();

		const auto& groups = Patches::All();
		if (groups.empty())
		{
			ImGuiMCP::TextDisabled("No patch groups are registered in this build.");
			RenderButtons();
			return;
		}

		for (const auto& g : groups)
		{
			ImGuiMCP::SeparatorText(g.name.c_str());
			ImGuiMCP::Text("%s", g.installed ? "Active" : "Not active");
			ImGuiMCP::TextWrapped("Touches: %s", g.touches.c_str());
			if (!g.installed)
			{
				ImGuiMCP::TextWrapped("Why: %s", g.status.c_str());
			}
			ImGuiMCP::Spacing();
		}

		RenderButtons();
	}

	void __stdcall DebugPanel::Render()
	{
		using namespace settings;

		ImGuiMCP::SeparatorText("Debug");

		ImGuiMCP::PushItemWidth(260.0F);

		int level = static_cast<int>(debug::logLevel);
		level = std::clamp(level, 0, kLogLevelCount - 1);
		if (ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount))
		{
			debug::logLevel = static_cast<std::uint32_t>(level);
			ApplyLogLevel();
		}
		HelpMarker("Applies immediately. The log is at Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.");

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		const auto s = Levelling::GetState();
		ImGuiMCP::Text("Level cost applied %llu time(s) this session", static_cast<unsigned long long>(s.applications));
		ImGuiMCP::Text("Live game settings: base %.1f, per level %.1f", s.liveBase, s.liveMult);

		RenderButtons();
	}
}
