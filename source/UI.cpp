#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Levelling.h"
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
				"igPopItemWidth"
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
