#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Compat.h"
#include "Difficulty.h"
#include "Enchanting.h"
#include "Levelling.h"
#include "Patches.h"
#include "Presets.h"
#include "SkillList.h"
#include "Skills.h"
#include "Settings.h"
#include "SkillPoints.h"

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
				"igPopID",
				"igInputText",
				"igGetFrameHeight",
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igInvisibleButton",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
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
					Enchanting::Apply();
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
					Enchanting::Apply();
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
		SKSEMenuFramework::AddSectionItem("Level Up", LevelUpPanel::Render);
		SKSEMenuFramework::AddSectionItem("Static Levelling", StaticLevellingPanel::Render);
		SKSEMenuFramework::AddSectionItem("Enchanting", EnchantingPanel::Render);
		SKSEMenuFramework::AddSectionItem("Presets", PresetsPanel::Render);
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
		if (ImGuiMCP::Toggle("Control the cost of a level", &over))
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
		if (ImGuiMCP::Toggle("Control skill caps", &over)) { skills::overrideCaps = over; }
		HelpMarker("Off by default. While it is off this mod asserts nothing about your skills - "
				   "with it off, not one instruction in the game is modified.");

		bool rates = skillexp::overrideRates;
		if (ImGuiMCP::Toggle("Control skill experience rates", &rates)) { skillexp::overrideRates = rates; }
		HelpMarker("Off by default. Turns on the per-skill experience multipliers below.");

		if (skillexp::overrideRates)
		{
			// Section 9's design rule: when another mod owns the income side, the setting that
			// depends on it is shown disabled WITH THE REASON, rather than silently doing nothing.
			if (Compat::AlternativeExperienceActive())
			{
				ImGuiMCP::TextDisabled("Skill increase -> level: not available");
				ImGuiMCP::TextWrapped("Experience is installed and owns where character experience "
									  "comes from, so this multiplier would do nothing. The level "
									  "COST on the Levelling tab is unaffected - Experience's own "
									  "page says mods editing those settings are compatible, and "
									  "recommends them.");
			}
			else
			{
				NudgeableSlider("Skill increase -> level", &skillexp::toLevelMult, 0.0F, 10.0F, "%.2f", 0.05F);
				HelpMarker("Multiplies what a skill increase pays toward your CHARACTER level. This is "
						   "the other side of the level cost on the Levelling tab: raise it and levels "
						   "come faster without changing what a level costs.");
			}
		}

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
			if (skillexp::overrideRates)
			{
				NudgeableSlider("Experience rate", &skillexp::mult[i], 0.0F, 10.0F, "%.2f", 0.05F);
				HelpMarker("Multiplies what one use of this skill pays toward it. 1.00 is vanilla; "
						   "below 1 is slower, above 1 is faster.");
			}

			ImGuiMCP::PopID();
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	// A tab whose engine patch is not active yet says so once, at the top, in plain words - the
	// settings below it are real and saved, they simply do nothing until the hook lands.
	void InertNotice(const char* a_group, const char* a_whatIsUnchanged)
	{
		if (Patches::IsInstalled(a_group)) { return; }
		ImGuiMCP::TextWrapped("Not active in this build: %s. The values below are remembered and "
							  "saved, so a configuration made now is ready the day it lands - see "
							  "the Patches tab.", a_whatIsUnchanged);
		ImGuiMCP::Spacing();
	}

	void __stdcall LevelUpPanel::Render()
	{
		using namespace settings;

		ImGuiMCP::TextWrapped("What a level up gives you: the perk points, and the health, magicka, "
							  "stamina and carry weight that come with the choice you make.");
		ImGuiMCP::Spacing();
		InertNotice("Attribute gains at level up", "the health/magicka/stamina and carry weight a level up grants are still vanilla");

		ImGuiMCP::PushItemWidth(260.0F);

		bool over = levelup::overrideRewards;
		if (ImGuiMCP::Toggle("Control what a level up grants", &over)) { levelup::overrideRewards = over; }
		HelpMarker("Off by default. While it is off this mod asserts nothing about level-up rewards.");

		if (levelup::overrideRewards)
		{
			ImGuiMCP::SeparatorText("Perk points per level");
			ImGuiMCP::TextWrapped("Whole perk points, as a table by level: from each listed level onward, that many per "
								   "level up. Vanilla is one row - from level 1, 1 perk.");
			{
				auto& rows = levelup::perksByLevel;
				int removeAt = -1;
				for (std::size_t i = 0; i < rows.size(); ++i)
				{
					ImGuiMCP::PushID(static_cast<int>(i));
					int from = rows[i].fromLevel;
					int perks = rows[i].perks;
					ImGuiMCP::PushItemWidth(120.0F);
					if (ImGuiMCP::InputInt("From level", &from)) { rows[i].fromLevel = static_cast<std::uint16_t>(std::clamp(from, 1, 1000)); }
					ImGuiMCP::SameLine();
					if (ImGuiMCP::InputInt("Perks", &perks)) { rows[i].perks = static_cast<std::uint8_t>(std::clamp(perks, 0, 20)); }
					ImGuiMCP::PopItemWidth();
					if (rows.size() > 1)
					{
						ImGuiMCP::SameLine();
						if (ImGuiMCP::SmallButton("Remove")) { removeAt = static_cast<int>(i); }
					}
					ImGuiMCP::PopID();
				}
				if (removeAt >= 0) { rows.erase(rows.begin() + removeAt); }
				if (ImGuiMCP::Button("Add a row"))
				{
					const auto last = rows.back();
					rows.push_back({ static_cast<std::uint16_t>(std::min(1000, last.fromLevel + 10)), last.perks });
				}
				std::sort(rows.begin(), rows.end(), [](const levelup::PerkRow& a, const levelup::PerkRow& b) { return a.fromLevel < b.fromLevel; });
			}
			HelpMarker("The last row at or below the level reached applies. Each row is a whole number of perks.");

			ImGuiMCP::SeparatorText("Attributes");
			NudgeableSlider("Health per level", &levelup::healthPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
			NudgeableSlider("Magicka per level", &levelup::magickaPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
			NudgeableSlider("Stamina per level", &levelup::staminaPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
			HelpMarker("What the chosen attribute gains. Vanilla is 10 for each.");

			ImGuiMCP::SeparatorText("Carry weight, per choice");
			NudgeableSlider("...when health is chosen", &levelup::carryWeightPerHealth, 0.0F, 50.0F, "%.1f", 1.0F);
			NudgeableSlider("...when magicka is chosen", &levelup::carryWeightPerMagicka, 0.0F, 50.0F, "%.1f", 1.0F);
			NudgeableSlider("...when stamina is chosen", &levelup::carryWeightPerStamina, 0.0F, 50.0F, "%.1f", 1.0F);
			HelpMarker("The cross terms. Vanilla gives 5 carry weight with stamina and none with the "
					   "other two.");
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	void __stdcall StaticLevellingPanel::Render()
	{
		using namespace settings;

		ImGuiMCP::TextWrapped("A fixed amount of experience per use of a skill, instead of vanilla's "
							  "scaling - so a skill advances at the same rate at level 5 and level 50.");
		ImGuiMCP::Spacing();
		InertNotice("Static levelling", "skill experience still scales the vanilla way");

		ImGuiMCP::PushItemWidth(200.0F);

		bool on = staticlevel::enabled;
		if (ImGuiMCP::Toggle("Use static skill levelling", &on)) { staticlevel::enabled = on; }
		HelpMarker("Off by default. While it is off, nothing about skill experience is asserted.");

		if (staticlevel::enabled)
		{
			ImGuiMCP::Spacing();
			ImGuiMCP::SeparatorText("Experience per use, as a percent of a skill level");
			HelpMarker("1.0 = one use is 1% of the level, so 100 uses raise the skill by one level whether it is at 5 or at 50. "
					   "Perk bonuses to skill use are not applied while this is on - the amount stays fixed.");
			for (int i = 0; i < skilllist::kCount; ++i)
			{
				ImGuiMCP::PushID(skilllist::kIniName[i]);
				NudgeableSlider(skilllist::kDisplayName[i], &staticlevel::xpPerUse[i], 0.0F, 100.0F, "%.2f", 0.25F);
				ImGuiMCP::PopID();
			}
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Skill points");
		ImGuiMCP::TextWrapped("Skills advance only by points spent in the level-up menu: each level grants points, the menu "
							  "shows every skill with + and -, and the choice applies when you pick your attribute. Needs the "
							  "skill-point level-up menu file (this mod ships one; Static Skill Leveling Rewritten's skins fit too).");
		bool pts = staticlevel::pointsEnabled;
		if (ImGuiMCP::Toggle("Use skill points", &pts)) { staticlevel::pointsEnabled = pts; }
		HelpMarker("Off by default. While on, ordinary skill experience is not banked - use only counts through points - and a "
				   "point-spent level pays nothing toward the character level. Takes effect after a restart.");
		if (staticlevel::pointsEnabled)
		{
			if (ImGuiMCP::InputInt("Points per level", &staticlevel::pointsPerLevel)) { staticlevel::pointsPerLevel = std::clamp(staticlevel::pointsPerLevel, 0, 500); }
			NudgeableSlider("Plus, per character level", &staticlevel::pointsLevelMult, -10.0F, 20.0F, "%.1f", 0.5F);
			HelpMarker("Points granted at a level = points per level + this x the level, whole numbers. Negative slows the curve.");
			if (ImGuiMCP::InputInt("Bank cap (0 = none)", &staticlevel::pointsCap)) { staticlevel::pointsCap = std::clamp(staticlevel::pointsCap, 0, 100000); }
			if (ImGuiMCP::InputInt("Most increases per skill, per level up", &staticlevel::maxIncreasesPerSkill)) { staticlevel::maxIncreasesPerSkill = std::clamp(staticlevel::maxIncreasesPerSkill, 1, 100); }
			ImGuiMCP::TextWrapped("Cost of one skill level, by the skill's current level:");
			const char* tiers[4] = { "Below 25", "25 to 49", "50 to 74", "75 and up" };
			for (int i = 0; i < 4; ++i) { if (ImGuiMCP::InputInt(tiers[i], &staticlevel::cost[i])) { staticlevel::cost[i] = std::clamp(staticlevel::cost[i], 1, 1000); } }
			if (auto* player = RE::PlayerCharacter::GetSingleton())
			{
				ImGuiMCP::Text("This character: %d point(s) banked; the next level grants %d.", SkillPoints::Bank(), SkillPoints::PointsForLevel(static_cast<std::uint16_t>(player->GetLevel() + 1)));
			}
			ImGuiMCP::TextWrapped("%s", SkillPoints::MenuStatus().c_str());
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	void __stdcall PresetsPanel::Render()
	{
		static char newName[64] = "My preset";
		static bool scanned = false;
		if (!scanned) { Presets::Refresh(); scanned = true; }

		ImGuiMCP::TextWrapped("A preset is a file in this mod's Presets folder, in the same format as its "
							  "INI. Whichever one is selected is what the mod is using - and the selection "
							  "belongs to THIS character, while the files are shared, so two saves can sit "
							  "on different presets at once.");
		ImGuiMCP::Spacing();

		const auto& all = Presets::All();
		ImGuiMCP::Text("This character is using: %s", Presets::Current().c_str());
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText("Game difficulty");
		bool follow = settings::difficulty::follow;
		if (ImGuiMCP::Toggle("Follow the game's difficulty", &follow))
		{
			settings::difficulty::follow = follow;
			OnMainThread([]() { statusMessage = Difficulty::OnFollowChanged(); });
		}
		HelpMarker("One configuration per difficulty. While this is on, the difficulty set in the game's own "
				   "Settings decides which of six presets is in use - Difficulty - Novice, Apprentice, Adept, Expert, "
				   "Master, Legendary - and changing it switches: what you had is saved into the old difficulty's "
				   "preset and the new one's is loaded (made from the current configuration the first time). "
				   "Saved with the mod's INI, so it stays on between sessions.");
		ImGuiMCP::Text("The game is on %s%s", Difficulty::CurrentName(),
					   settings::difficulty::follow ? (" -> preset \"" + Difficulty::PresetNameFor(Difficulty::Current()) + "\"").c_str() : "");
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText("Presets");
		for (const auto& name : all)
		{
			ImGuiMCP::PushID(name.c_str());
			const bool isCurrent = (name == Presets::Current());
			if (ImGuiMCP::Button(isCurrent ? "In use" : "Use"))
			{
				const std::string picked = name;
				OnMainThread([picked]() {
					statusMessage = Presets::Select(picked)
										? "Now using " + picked + "."
										: "Could not read that preset - fell back to the built-in default.";
				});
			}
			ImGuiMCP::SameLine();
			ImGuiMCP::Text("%s", name.c_str());
			ImGuiMCP::PopID();
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("This configuration");

		if (ImGuiMCP::Button("Save into the selected preset"))
		{
			OnMainThread([]() {
				statusMessage = Presets::SaveCurrent()
									? "Saved into " + Presets::Current() + "."
									: "The built-in default is not a file - use Save as a new preset instead.";
			});
		}
		HelpMarker("A preset is a living configuration, not a read-only template: this writes what is on "
				   "the pages now back into the preset you are using.");

		ImGuiMCP::PushItemWidth(200.0F);
		ImGuiMCP::InputText("New preset name", newName, sizeof(newName));
		ImGuiMCP::PopItemWidth();
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Save as a new preset"))
		{
			const std::string wanted = newName;
			OnMainThread([wanted]() {
				statusMessage = Presets::Export(wanted)
									? "Wrote and selected " + wanted + "."
									: "That name cannot be used as a file name.";
			});
		}
		HelpMarker("Writes the current configuration out as its own file, which is also how you share one.");

		if (Presets::Current() != Presets::kDefaultName)
		{
			if (ImGuiMCP::Button("Delete the selected preset"))
			{
				const std::string doomed = Presets::Current();
				OnMainThread([doomed]() {
					statusMessage = Presets::Delete(doomed)
										? "Deleted " + doomed + "; back on the built-in default."
										: "Could not delete that preset.";
				});
			}
			HelpMarker("Deletes the file. The built-in default can never be deleted, so there is always "
					   "somewhere to fall back to.");
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::Text("%s", Presets::Dir().c_str());

		RenderButtons();
	}

	void __stdcall EnchantingPanel::Render()
	{
		using namespace settings;

		const auto& group = Enchanting::Settings();

		ImGuiMCP::TextWrapped("How the cost of using an enchanted item scales with your Enchanting skill. "
							  "This is the equation that breaks when Enchanting is uncapped, which is why it "
							  "sits beside the skill caps.");
		ImGuiMCP::Spacing();

		if (!group.Captured() || group.FoundCount() == 0)
		{
			ImGuiMCP::TextWrapped("None of the enchanting cost settings could be read on this runtime, so "
								  "this tab will not write anything. See the log.");
			RenderButtons();
			return;
		}
		if (group.FoundCount() < static_cast<int>(group.Entries().size()))
		{
			ImGuiMCP::TextWrapped("Only some of these settings exist on this runtime; the rest are left alone.");
			ImGuiMCP::Spacing();
		}

		ImGuiMCP::PushItemWidth(260.0F);

		bool changed = false;
		bool over = enchanting::overrideCost;
		if (ImGuiMCP::Toggle("Control the enchantment charge cost", &over))
		{
			enchanting::overrideCost = over;
			changed = true;
		}
		HelpMarker("Off by default. While it is off this mod restores the values your install came with "
				   "and leaves them alone.");

		if (enchanting::overrideCost)
		{
			changed |= NudgeableSlider("Cost base", &enchanting::costBase, 0.0F, 100.0F, "%.2f", 0.5F);
			changed |= NudgeableSlider("Cost scale", &enchanting::costScale, 0.0F, 10.0F, "%.2f", 0.05F);
			changed |= NudgeableSlider("Cost multiplier", &enchanting::costMult, 0.0F, 100.0F, "%.2f", 0.5F);
			changed |= NudgeableSlider("Cost exponent", &enchanting::costExponent, 0.0F, 5.0F, "%.2f", 0.05F);
			HelpMarker("Lower values make an enchanted item cheaper to use. The exponent is the one that "
					   "runs away when the skill climbs past 100.");
		}

		if (changed) { Enchanting::RequestApply(); }

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("What the game is using");
		const auto& entries = group.Entries();
		for (int i = 0; i < static_cast<int>(entries.size()); ++i)
		{
			if (!entries[static_cast<std::size_t>(i)].found)
			{
				ImGuiMCP::TextDisabled("%s - not on this runtime", entries[static_cast<std::size_t>(i)].name);
				continue;
			}
			ImGuiMCP::Text("%s = %.3f   (this install: %.3f)", entries[static_cast<std::size_t>(i)].name,
						   group.Live(i), entries[static_cast<std::size_t>(i)].vanilla);
		}

		RenderButtons();
	}

	void __stdcall PatchesPanel::Render()
	{
		ImGuiMCP::TextWrapped("Each engine patch this mod installs, and what it changes. A patch that is "
							  "not active is not a fault - that part of the game simply behaves as it "
							  "would without this mod.");
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText("What else is installed");
		ImGuiMCP::TextWrapped("Detected at load. A conflict found here switches OUR feature off - "
							  "never another mod's - and every switch stays yours to override.");
		ImGuiMCP::Spacing();
		for (const auto& d : Compat::All())
		{
			if (d.present) { ImGuiMCP::Text("%s: installed", d.name.c_str()); }
			else { ImGuiMCP::TextDisabled("%s: not installed", d.name.c_str()); }
			ImGuiMCP::TextWrapped("    %s", d.consequence.c_str());
		}
		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText("Engine patches");

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
