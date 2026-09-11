#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Attributes.h"
#include "CarryWeight.h"
#include "Compat.h"
#include "Difficulty.h"
#include "DifficultyValues.h"
#include "Enchanting.h"
#include "ExperienceSources.h"
#include "Levelling.h"
#include "Patches.h"
#include "Presets.h"
#include "SkillList.h"
#include "Skills.h"
#include "Settings.h"
#include "SkillPoints.h"

#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <format>
#include <functional>
#include <string>
#include <vector>

namespace UI
{
	namespace
	{
		std::string statusMessage;
		std::string selectedSlider;

		// Option lists and label tables are parallel key/label arrays: the key array carries the
		// translation key, the label array the compiled English fallback. ComboTR() pairs them by
		// index and rebuilds a combo's option list from TR'd entries every frame; a table's rows are
		// translated at the use site (translation rollout plan, section 2.2). Only drawn text is in
		// these arrays - never an INI key, a game setting name or a preset file name.
		constexpr const char* const kLogLevelKeys[] = { "CPC_Log_Trace", "CPC_Log_Debug", "CPC_Log_Info", "CPC_Log_Warning", "CPC_Log_Error", "CPC_Log_Critical", "CPC_Log_Off" };
		constexpr const char* const kLogLevelLabels[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount = 7;

		// The eighteen skills in SkillList.h's order; the labels mirror skilllist::kDisplayName so the
		// whole English list can be read out of this one file by the generator.
		constexpr const char* const kSkillKeys[] = {
			"CPC_Skill_OneHanded", "CPC_Skill_TwoHanded", "CPC_Skill_Archery", "CPC_Skill_Block", "CPC_Skill_Smithing", "CPC_Skill_HeavyArmor",
			"CPC_Skill_LightArmor", "CPC_Skill_Pickpocket", "CPC_Skill_Lockpicking", "CPC_Skill_Sneak", "CPC_Skill_Alchemy", "CPC_Skill_Speech",
			"CPC_Skill_Alteration", "CPC_Skill_Conjuration", "CPC_Skill_Destruction", "CPC_Skill_Illusion", "CPC_Skill_Restoration", "CPC_Skill_Enchanting"
		};
		constexpr const char* const kSkillLabels[] = {
			"One-handed", "Two-handed", "Archery", "Block", "Smithing", "Heavy Armor",
			"Light Armor", "Pickpocket", "Lockpicking", "Sneak", "Alchemy", "Speech",
			"Alteration", "Conjuration", "Destruction", "Illusion", "Restoration", "Enchanting"
		};
		static_assert(sizeof(kSkillKeys) / sizeof(kSkillKeys[0]) == static_cast<std::size_t>(skilllist::kCount));

		constexpr const char* const kAttributeKeys[] = { "CPC_Attribute_Health", "CPC_Attribute_Magicka", "CPC_Attribute_Stamina" };
		constexpr const char* const kAttributeLabels[] = { "Health", "Magicka", "Stamina" };

		// The six difficulties: the Difficulty tab names them and so does the Presets tab.
		constexpr const char* const kDifficultyKeys[] = { "CPC_Difficulty_Novice", "CPC_Difficulty_Apprentice", "CPC_Difficulty_Adept", "CPC_Difficulty_Expert", "CPC_Difficulty_Master", "CPC_Difficulty_Legendary" };
		constexpr const char* const kDifficultyLabels[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };

		// A Combo whose option list is rebuilt from TR'd entries every frame: store owns the text
		// for the duration of the call, items is the array of pointers ImGui wants.
		bool ComboTR(const char* a_label, int* a_current, const char* const* a_keys, const char* const* a_labels, int a_count)
		{
			std::vector<std::string> store;
			store.reserve(static_cast<std::size_t>(a_count));
			for (int i = 0; i < a_count; ++i) { store.emplace_back(strings::TR(a_keys[i], a_labels[i])); }
			std::vector<const char*> items;
			items.reserve(store.size());
			for (const auto& s : store) { items.push_back(s.c_str()); }
			return ImGuiMCP::Combo(a_label, a_current, items.data(), a_count);
		}

		// A translated printf format applied at runtime, for the few places that built their text
		// with std::format before. The check script enforces that every translation of a key carries
		// the same specifiers in the same order, so this is as safe as the compiled literal was.
		std::string TrFormat(const char* a_format, ...)
		{
			char buf[1024];
			va_list args;
			va_start(args, a_format);
			const int written = std::vsnprintf(buf, sizeof(buf), a_format ? a_format : "", args);
			va_end(args);
			return written > 0 ? std::string(buf) : std::string{};
		}

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

			if (ImGuiMCP::Button(strings::TR("CPC_Btn_Save", "Save")))
			{
				statusMessage = strings::TR("CPC_Status_Saving", "Saving...");
				OnMainThread([]() {
					statusMessage = settings::Save() ? strings::TR("CPC_Status_Saved", "Settings saved.") : strings::TR("CPC_Status_SaveFailed", "Could not write the INI. See the log for why.");
				});
			}
			HelpMarker(strings::TR("CPC_Help_Save", "Writes every setting on these pages to the plugin's INI so it survives a restart."));

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button(strings::TR("CPC_Btn_Reload", "Reload from INI")))
			{
				statusMessage = strings::TR("CPC_Status_Reloading", "Reloading...");
				OnMainThread([]() {
					const bool ok = settings::Reload();
					Levelling::Apply();
					Enchanting::Apply();
					Attributes::Apply();
					CarryWeight::Apply();
					DifficultyValues::Apply();
					statusMessage = ok ? strings::TR("CPC_Status_Reloaded", "Settings reloaded from the INI.")
									   : strings::TR("CPC_Status_ReloadFailed", "Could not read the INI. See the log for why.");
				});
			}
			HelpMarker(strings::TR("CPC_Help_Reload", "Throws away any change made here since the last save and re-reads the INI from disk."));

			ImGuiMCP::SameLine();

			if (ImGuiMCP::Button(strings::TR("CPC_Btn_Defaults", "Restore defaults")))
			{
				OnMainThread([]() {
					settings::RestoreDefaults();
					Levelling::Apply();
					Enchanting::Apply();
					Attributes::Apply();
					CarryWeight::Apply();
					DifficultyValues::Apply();
					logger::debug("Restored default settings");
				});
				statusMessage = strings::TR("CPC_Status_DefaultsRestored", "Defaults restored - the values this install had before the mod. Press Save to keep them.");
			}
			HelpMarker(strings::TR("CPC_Help_Defaults", "Puts every setting back to the value this installation had before the mod touched it. Nothing is written until you press Save."));

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
		SKSEMenuFramework::AddSectionItem("Experience", ExperiencePanel::Render);
		SKSEMenuFramework::AddSectionItem("Skills", SkillsPanel::Render);
		SKSEMenuFramework::AddSectionItem("Level Up", LevelUpPanel::Render);
		SKSEMenuFramework::AddSectionItem("Attributes", AttributesPanel::Render);
		SKSEMenuFramework::AddSectionItem("Carry Weight", CarryWeightPanel::Render);
		SKSEMenuFramework::AddSectionItem("Difficulty", DifficultyPanel::Render);
		SKSEMenuFramework::AddSectionItem("Static Levelling", StaticLevellingPanel::Render);
		SKSEMenuFramework::AddSectionItem("Enchanting", EnchantingPanel::Render);
		SKSEMenuFramework::AddSectionItem("Presets", PresetsPanel::Render);
		SKSEMenuFramework::AddSectionItem("Patches", PatchesPanel::Render);
		SKSEMenuFramework::AddSectionItem("Debug", DebugPanel::Render);
		logger::info("Registered the settings pages with the menu framework");
	}

	void __stdcall LevellingPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		const auto s = Levelling::GetState();

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Lvl_Intro", "What one character level costs. Skyrim works out the experience needed for "
							  "your next level as:  base + (per-level x your level)."));
		ImGuiMCP::Spacing();

		if (!s.captured)
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Lvl_NotCaptured", "This install's own level-cost settings could not be read, so this tab will "
								  "not write anything. See the log."));
			RenderButtons();
			return;
		}

		ImGuiMCP::PushItemWidth(260.0F);

		ImGuiMCP::SeparatorText(strings::TR("CPC_Lvl_SecCost", "Cost of a level"));

		bool changed = false;
		bool over = levelling::overrideCost;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Lvl_Control", "Control the cost of a level"), &over))
		{
			levelling::overrideCost = over;
			changed = true;
		}
		HelpMarker(strings::TR("CPC_Lvl_HelpControl", "Off by default, and while it is off this mod writes nothing - your game levels exactly "
				   "as it did before, and any other mod that sets these values keeps them. Turn it on and the "
				   "two values below are applied and re-applied every time a save loads."));

		if (levelling::overrideCost)
		{
			changed |= NudgeableSlider(strings::TR("CPC_Lvl_Base", "Base cost"), &levelling::base, 0.0F, 2000.0F, "%.0f", 5.0F);
			HelpMarker(strings::TR("CPC_Lvl_HelpBase", "The flat part of the cost, paid at every level. This install's own value is shown below."));

			changed |= NudgeableSlider(strings::TR("CPC_Lvl_Mult", "Per level"), &levelling::mult, 0.0F, 500.0F, "%.1f", 1.0F);
			HelpMarker(strings::TR("CPC_Lvl_HelpMult", "Added to the cost for each level you already have. Raising this makes later levels "
					   "progressively slower; lowering it flattens the curve."));
		}

		if (changed) { Levelling::RequestApply(); }

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Sec_RightNow", "What that means right now"));

		// The readout is what proves the setting took - one of the plan's two rules for a tab.
		ImGuiMCP::Text(strings::TR("CPC_Lvl_CostLine", "Level %u to %u costs %.0f experience  (%.0f + %.1f x %u)"),
					   s.playerLevel, s.playerLevel + 1, s.costThisLevel,
					   s.liveBase, s.liveMult, s.playerLevel);
		ImGuiMCP::Text(strings::TR("CPC_Lvl_VanillaLine", "This install's own values: %.0f base, %.1f per level"), s.vanillaBase, s.vanillaMult);
		if (!s.overriding)
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Lvl_NotControlling", "Not controlling the cost - the values above are whatever the game is using."));
		}

		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button(strings::TR("CPC_Btn_ApplyNow", "Apply now")))
		{
			Levelling::RequestApply();
			statusMessage = strings::TR("CPC_Status_Applied", "Applied.");
		}
		HelpMarker(strings::TR("CPC_Lvl_HelpApply", "Re-writes the values. It also runs by itself when a save loads, when a new game starts, "
				   "and when you change anything above - nothing runs in the background."));

		RenderButtons();
	}

	void __stdcall SkillsPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		const auto s = Skills::GetState();
		const bool capsActive = Patches::IsInstalled("Skill caps");

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Skl_Intro", "Where each skill stops, and what the game's own formulas read for it - "
							  "so a skill can show 300 while combat maths still treats it as 100."));
		ImGuiMCP::Spacing();

		if (!capsActive)
		{
			// Said plainly rather than left to be discovered. The values below are still stored
			// and saved, so a configuration made now is ready the day the patch lands.
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Skl_CapsInert", "The skill cap patch is NOT active in this build, so every skill still "
								  "stops at 100 exactly as in vanilla. The values below are remembered "
								  "and saved, but they do nothing yet. See the Patches tab."));
			ImGuiMCP::Spacing();
		}

		ImGuiMCP::PushItemWidth(200.0F);

		bool over = skills::overrideCaps;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Skl_ControlCaps", "Control skill caps"), &over)) { skills::overrideCaps = over; }
		HelpMarker(strings::TR("CPC_Skl_HelpCaps", "Off by default. While it is off this mod asserts nothing about your skills - "
				   "with it off, not one instruction in the game is modified."));

		bool rates = skillexp::overrideRates;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Skl_ControlRates", "Control skill experience rates"), &rates)) { skillexp::overrideRates = rates; }
		HelpMarker(strings::TR("CPC_Skl_HelpRates", "Off by default. Turns on the per-skill experience multipliers below."));

		if (skillexp::overrideRates)
		{
			// Section 9's design rule: when another mod owns the income side, the setting that
			// depends on it is shown disabled WITH THE REASON, rather than silently doing nothing.
			if (Compat::AlternativeExperienceActive())
			{
				ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Skl_ToLevelUnavailable", "Skill increase -> level: not available"));
				ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Skl_ExperienceOwns", "Experience is installed and owns where character experience "
									  "comes from, so this multiplier would do nothing. The level "
									  "COST on the Levelling tab is unaffected - Experience's own "
									  "page says mods editing those settings are compatible, and "
									  "recommends them."));
			}
			else
			{
				NudgeableSlider(strings::TR("CPC_Skl_ToLevel", "Skill increase -> level"), &skillexp::toLevelMult, 0.0F, 10.0F, "%.2f", 0.05F);
				HelpMarker(strings::TR("CPC_Skl_HelpToLevel", "Multiplies what a skill increase pays toward your CHARACTER level. This is "
						   "the other side of the level cost on the Levelling tab: raise it and levels "
						   "come faster without changing what a level costs."));
			}
		}

		// Legendary skills (1.1.5): when a skill may be made legendary, what it drops to, and the hint.
		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Skl_SecLegendary", "Legendary skills"));

		bool leg = legendary::control;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Skl_ControlLegendary", "Control legendary skills"), &leg)) { legendary::control = leg; }
		HelpMarker(strings::TR("CPC_Skl_HelpLegendary", "Off by default. While it is off, making a skill legendary works exactly as in "
				   "vanilla. Turning it on takes effect after a restart; the values below apply the next time the Skills menu opens."));

		if (legendary::control)
		{
			const bool resetOn = Patches::IsInstalled("Legendary reset level");
			const bool thresholdOn = Patches::IsInstalled("Legendary threshold");
			const bool buttonOn = Patches::IsInstalled("Legendary button");
			if (!resetOn && !thresholdOn && !buttonOn)
			{
				ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Skl_LegInert", "Not active yet: restart the game with this switched on. "
									  "The Patches tab says which parts attached."));
			}
			else if (!resetOn || !thresholdOn || !buttonOn)
			{
				ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Skl_LegPartial", "Only part of this is active on this game version - "
									  "the Patches tab says which."));
			}

			NudgeableSlider(strings::TR("CPC_Skl_LegThreshold", "Legendary at level"), &legendary::threshold, 15.0F, 1000.0F, "%.0f", 5.0F);
			HelpMarker(strings::TR("CPC_Skl_HelpLegThreshold", "The skill level at which a skill can be made legendary. 100 is vanilla. "
					   "Below 100 the Skills menu does not show the Legendary hint, but the legendary key (SPACE) still works."));

			NudgeableSlider(strings::TR("CPC_Skl_LegAfter", "Level after legendary"), &legendary::levelAfter, 0.0F, 1000.0F, "%.0f", 5.0F);
			HelpMarker(strings::TR("CPC_Skl_HelpLegAfter", "The level a skill drops to when it is made legendary. 0 uses the game's own "
					   "value (15). Making a skill legendary never raises it."));

			bool keep = legendary::keepLevel;
			if (ImGuiMCP::Toggle(strings::TR("CPC_Skl_LegKeep", "Keep the level when made legendary"), &keep)) { legendary::keepLevel = keep; }
			HelpMarker(strings::TR("CPC_Skl_HelpLegKeep", "The skill keeps its level instead of dropping - the setting above is then ignored."));

			bool hide = legendary::hideButton;
			if (ImGuiMCP::Toggle(strings::TR("CPC_Skl_LegHide", "Hide the Legendary button"), &hide)) { legendary::hideButton = hide; }
			HelpMarker(strings::TR("CPC_Skl_HelpLegHide", "Hides the Legendary hint in the Skills menu. The legendary key (SPACE) still works."));
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Skl_SecNow", "Your skills right now"));

		if (!s.readable)
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Skl_NoCharacter", "No character loaded - load a save to see your skills."));
			ImGuiMCP::PopItemWidth();
			RenderButtons();
			return;
		}

		// The live readout: the game's own numbers, straight out of the player's progression
		// data. It is also the check on the Levelling tab - the character threshold below is
		// what the game is really holding.
		ImGuiMCP::Text(strings::TR("CPC_Skl_CharacterLine", "Character level %u - %.0f of %.0f experience toward the next level"),
					   s.characterLevel, s.characterXp, s.characterThreshold);
		ImGuiMCP::Spacing();

		for (int i = 0; i < skilllist::kCount; ++i)
		{
			ImGuiMCP::PushID(skilllist::kIniName[i]);

			ImGuiMCP::Text(strings::TR("CPC_Skl_Row", "%-12s  %5.1f   %.0f / %.0f"), strings::TR(kSkillKeys[i], kSkillLabels[i]),
						   s.skill[i].level, s.skill[i].xp, s.skill[i].levelThreshold);

			if (skills::overrideCaps)
			{
				NudgeableSlider(strings::TR("CPC_Skl_Cap", "Cap"), &skills::cap[i], 100.0F, 1000.0F, "%.0f", 5.0F);
				HelpMarker(strings::TR("CPC_Skl_HelpCap", "The level this skill stops advancing at. 100 is vanilla."));

				NudgeableSlider(strings::TR("CPC_Skl_FormulaCap", "Formula cap"), &skills::formulaCap[i], 10.0F, 1000.0F, "%.0f", 5.0F);
				HelpMarker(strings::TR("CPC_Skl_HelpFormulaCap", "The value the game's own calculations use for this skill, however high "
						   "the skill itself reads. Leaving this at 100 keeps combat and prices "
						   "balanced while the skill number keeps climbing."));
			}
			if (skillexp::overrideRates)
			{
				NudgeableSlider(strings::TR("CPC_Skl_Rate", "Experience rate"), &skillexp::mult[i], 0.0F, 10.0F, "%.2f", 0.05F);
				HelpMarker(strings::TR("CPC_Skl_HelpRate", "Multiplies what one use of this skill pays toward it. 1.00 is vanilla; "
						   "below 1 is slower, above 1 is faster."));
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
		ImGuiMCP::TextWrapped(strings::TR("CPC_InertNotice", "Not active in this build: %s. The values below are remembered and "
							  "saved, so a configuration made now is ready the day it lands - see "
							  "the Patches tab."), a_whatIsUnchanged);
		ImGuiMCP::Spacing();
	}

	void __stdcall LevelUpPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_LvlUp_Intro", "What a level up gives you: the perk points, and the health, magicka, "
							  "stamina and carry weight that come with the choice you make."));
		ImGuiMCP::Spacing();
		InertNotice("Attribute gains at level up", strings::TR("CPC_LvlUp_Inert", "the health/magicka/stamina and carry weight a level up grants are still vanilla; the perk table is separate"));

		ImGuiMCP::PushItemWidth(260.0F);

		bool over = levelup::overrideRewards;
		if (ImGuiMCP::Toggle(strings::TR("CPC_LvlUp_Control", "Control what a level up grants"), &over)) { levelup::overrideRewards = over; }
		HelpMarker(strings::TR("CPC_LvlUp_HelpControl", "Off by default. While it is off this mod asserts nothing about level-up rewards."));

		if (levelup::overrideRewards)
		{
			ImGuiMCP::SeparatorText(strings::TR("CPC_LvlUp_SecPerks", "Perk points per level"));
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_LvlUp_PerkIntro", "Whole perk points, as a table by level: from each listed level onward, that many per "
								   "level up. Vanilla is one row - from level 1, 1 perk."));
			{
				auto& rows = levelup::perksByLevel;
				int removeAt = -1;
				for (std::size_t i = 0; i < rows.size(); ++i)
				{
					ImGuiMCP::PushID(static_cast<int>(i));
					int from = rows[i].fromLevel;
					int perks = rows[i].perks;
					ImGuiMCP::PushItemWidth(120.0F);
					if (ImGuiMCP::InputInt(strings::TR("CPC_LvlUp_FromLevel", "From level"), &from)) { rows[i].fromLevel = static_cast<std::uint16_t>(std::clamp(from, 1, 1000)); }
					ImGuiMCP::SameLine();
					if (ImGuiMCP::InputInt(strings::TR("CPC_LvlUp_Perks", "Perks"), &perks)) { rows[i].perks = static_cast<std::uint8_t>(std::clamp(perks, 0, 20)); }
					ImGuiMCP::PopItemWidth();
					if (rows.size() > 1)
					{
						ImGuiMCP::SameLine();
						if (ImGuiMCP::SmallButton(strings::TR("CPC_LvlUp_Remove", "Remove"))) { removeAt = static_cast<int>(i); }
					}
					ImGuiMCP::PopID();
				}
				if (removeAt >= 0) { rows.erase(rows.begin() + removeAt); }
				if (ImGuiMCP::Button(strings::TR("CPC_LvlUp_AddRow", "Add a row")))
				{
					const auto last = rows.back();
					rows.push_back({ static_cast<std::uint16_t>(std::min(1000, last.fromLevel + 10)), last.perks });
				}
				std::sort(rows.begin(), rows.end(), [](const levelup::PerkRow& a, const levelup::PerkRow& b) { return a.fromLevel < b.fromLevel; });
			}
			HelpMarker(strings::TR("CPC_LvlUp_HelpRows", "The last row at or below the level reached applies. Each row is a whole number of perks."));

			ImGuiMCP::Spacing();
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_LvlUp_SeeOtherTabs", "The health, magicka and stamina a level up grants are on the Attributes tab, and the carry weight per choice is on the Carry Weight tab; both apply while this is on."));
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	void __stdcall AttributesPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Attr_Intro", "Starting health, magicka and stamina, and what each gains on a level up where you choose it. "
							  "The game keeps no count of those choices, so this mod counts every one itself from the moment "
							  "it is installed on a character."));
		ImGuiMCP::Spacing();
		InertNotice("Attribute gains at level up", strings::TR("CPC_Attr_Inert", "attribute choices are not being counted and a level up grants what vanilla grants"));

		ImGuiMCP::PushItemWidth(260.0F);
		bool changed = false;

		bool control = attributes::control;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Attr_Control", "Control starting attributes"), &control)) { attributes::control = control; changed = true; }
		HelpMarker(strings::TR("CPC_Attr_HelpControl", "Off by default. On: each starting value below is applied on top of your race's own start as this "
				   "mod's permanent modifier, and taken away again if you turn this off. Nothing is asserted while it is off."));

		if (attributes::control)
		{
			ImGuiMCP::SeparatorText(strings::TR("CPC_Attr_SecStarting", "Starting values"));
			changed |= NudgeableSlider(strings::TR("CPC_Attr_StartHealth", "Starting health"), &attributes::starting[0], 10.0F, 1000.0F, "%.0f", 10.0F);
			changed |= NudgeableSlider(strings::TR("CPC_Attr_StartMagicka", "Starting magicka"), &attributes::starting[1], 10.0F, 1000.0F, "%.0f", 10.0F);
			changed |= NudgeableSlider(strings::TR("CPC_Attr_StartStamina", "Starting stamina"), &attributes::starting[2], 10.0F, 1000.0F, "%.0f", 10.0F);
			HelpMarker(strings::TR("CPC_Attr_HelpStarting", "What the attribute starts at, for a character whose vanilla start is 100. A race that starts higher or "
					   "lower keeps its difference: the value is applied as (this - 100) on top of what the character started with."));
		}

		ImGuiMCP::SeparatorText(strings::TR("CPC_Attr_SecGain", "Gain per level up"));
		NudgeableSlider(strings::TR("CPC_Attr_HealthPerLevel", "Health per level"), &levelup::healthPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
		NudgeableSlider(strings::TR("CPC_Attr_MagickaPerLevel", "Magicka per level"), &levelup::magickaPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
		NudgeableSlider(strings::TR("CPC_Attr_StaminaPerLevel", "Stamina per level"), &levelup::staminaPerLevel, 0.0F, 100.0F, "%.0f", 1.0F);
		HelpMarker(strings::TR("CPC_Attr_HelpGain", "What the chosen attribute gains. Vanilla is 10 for each."));
		if (!levelup::overrideRewards)
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Attr_NeedsLevelUpControl", "Applied while \"Control what a level up grants\" on the Level Up tab is on. It is off, so a level up grants vanilla's 10."));
		}

		if (changed) { Attributes::RequestApply(); }
		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Sec_RightNow", "What that means right now"));
		const auto s = Attributes::GetState();
		if (!s.haveHistory) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_NoCharacter", "No character loaded yet.")); }
		else
		{
			// The three attribute names are kAttributeKeys/kAttributeLabels in the anonymous
			// namespace above, translated at the use site below.
			for (int i = 0; i < 3; ++i)
			{
				const auto& r = s.row[i];
				// The count's noun is a WORD, not an English "s" suffix: a plural marker glued onto
				// a translated sentence is wrong in most of the ten languages, and the drawn
				// English reads exactly as it did before.
				ImGuiMCP::Text(strings::TR("CPC_Attr_Row", "%s: %.0f base %+.0f from this mod %+.0f over %u %s = %.0f permanent   (now %.0f)"),
							   strings::TR(kAttributeKeys[i], kAttributeLabels[i]), r.base, r.applied, r.gained, r.invested,
							   r.invested == 1 ? strings::TR("CPC_Attr_Investment", "investment") : strings::TR("CPC_Attr_Investments", "investments"),
							   r.permanent, r.current);
			}
			if (s.sinceLevel > 1)
			{
				ImGuiMCP::TextWrapped(strings::TR("CPC_Attr_SinceLevel", "Counted since level %u, when this mod first saw this character. The %u earlier level-ups are unknown "
									  "and are not guessed at: they are part of the base, with whatever else the character started with."),
									  s.sinceLevel, s.sinceLevel - 1);
			}
			else { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Attr_SinceLevelOne", "Counted since level 1 - the whole history is known.")); }
			if (!Attributes::Counting()) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Attr_NotCounting", "Not counting: the level-up patch is not attached (see the Patches tab).")); }
		}
		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button(strings::TR("CPC_Btn_ApplyNow", "Apply now"))) { Attributes::RequestApply(); statusMessage = strings::TR("CPC_Status_Applied", "Applied."); }
		HelpMarker(strings::TR("CPC_Attr_HelpApply", "Re-applies the starting values and refreshes the readout. It also runs by itself when a save loads, at each "
				   "level up, and when you change anything above - nothing runs in the background."));

		RenderButtons();
	}

	void __stdcall CarryWeightPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_CW_Intro", "Carry weight as a formula of your level: starting + per level x (level - 1), applied to your permanent "
							  "carry weight (enchantments and spells are left alone). It is recalculated when a save loads, when you "
							  "level up, when you change a value here, and when you press Apply now - nothing runs in the background."));
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);
		bool changed = false;
		bool control = carryweight::control;
		if (ImGuiMCP::Toggle(strings::TR("CPC_CW_Control", "Control carry weight"), &control)) { carryweight::control = control; changed = true; }
		HelpMarker(strings::TR("CPC_CW_HelpControl", "Off by default. While it is off nothing is asserted, and anything this mod had added is taken away again."));

		const auto s = CarryWeight::GetState();
		if (carryweight::control && !s.standDown.empty())
		{
			ImGuiMCP::TextWrapped(strings::TR("CPC_CW_StandingDown", "Standing down: %s. Nothing here is applied while it is."), s.standDown.c_str());
		}
		if (carryweight::control)
		{
			ImGuiMCP::SeparatorText(strings::TR("CPC_CW_SecFormula", "The formula"));
			changed |= NudgeableSlider(strings::TR("CPC_CW_Starting", "Starting carry weight"), &carryweight::starting, 0.0F, 2000.0F, "%.0f", 10.0F);
			HelpMarker(strings::TR("CPC_CW_HelpStarting", "Carry weight at level 1. Vanilla is 300."));
			changed |= NudgeableSlider(strings::TR("CPC_CW_PerLevel", "Per level"), &carryweight::perLevel, 0.0F, 50.0F, "%.1f", 0.5F);
			HelpMarker(strings::TR("CPC_CW_HelpPerLevel", "Added for every level after the first, whichever attribute you choose."));
		}

		ImGuiMCP::SeparatorText(strings::TR("CPC_CW_SecPerChoice", "Per choice at level up"));
		if (carryweight::control)
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("CPC_CW_OffWhileFormula", "Off while the formula controls carry weight: it sets the total, so a per-choice gain would be undone at the next recalculation."));
		}
		else
		{
			NudgeableSlider(strings::TR("CPC_CW_PerHealth", "...when health is chosen"), &levelup::carryWeightPerHealth, 0.0F, 50.0F, "%.1f", 1.0F);
			NudgeableSlider(strings::TR("CPC_CW_PerMagicka", "...when magicka is chosen"), &levelup::carryWeightPerMagicka, 0.0F, 50.0F, "%.1f", 1.0F);
			NudgeableSlider(strings::TR("CPC_CW_PerStamina", "...when stamina is chosen"), &levelup::carryWeightPerStamina, 0.0F, 50.0F, "%.1f", 1.0F);
			HelpMarker(strings::TR("CPC_CW_HelpCross", "The cross terms. Vanilla gives 5 carry weight with stamina and none with the other two."));
			if (!levelup::overrideRewards)
			{
				ImGuiMCP::TextDisabled("%s", strings::TR("CPC_CW_NeedsLevelUpControl", "Applied while \"Control what a level up grants\" on the Level Up tab is on. It is off, so stamina gives vanilla's 5."));
			}
			if (Compat::CarryWeightOwnedElsewhere()) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_CW_Zeroed", "Zeroed: one of our standalone carry-weight mods is loaded and owns carry weight.")); }
		}
		if (changed) { CarryWeight::RequestApply(); }
		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Sec_RightNow", "What that means right now"));
		if (s.playerLevel == 0) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_NoCharacter", "No character loaded yet.")); }
		else
		{
			if (s.controlling)
			{
				ImGuiMCP::Text(strings::TR("CPC_CW_FormulaLine", "Level %u: %.0f + %.1f x %u = %.0f"), static_cast<unsigned>(s.playerLevel), carryweight::starting, carryweight::perLevel,
							   static_cast<unsigned>(s.playerLevel > 0 ? s.playerLevel - 1 : 0), s.target);
			}
			ImGuiMCP::Text(strings::TR("CPC_CW_PermanentLine", "Permanent carry weight %.0f, now %.0f with temporary effects; net added by this mod %.0f"), s.permanent, s.current, s.applied);
			if (!s.controlling) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_CW_NotControlling", "Not controlling carry weight - the values above are whatever the game is using.")); }
		}
		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button(strings::TR("CPC_Btn_ApplyNow", "Apply now"))) { CarryWeight::RequestApply(); statusMessage = strings::TR("CPC_Status_Applied", "Applied."); }
		HelpMarker(strings::TR("CPC_CW_HelpApply", "Recalculates now. It also runs by itself when a save loads, at each level up, and when you change anything above."));

		RenderButtons();
	}

	void __stdcall DifficultyPanel::Render()
	{
		strings::Tick();

		using namespace settings;
		// The six difficulty names are kDifficultyKeys/kDifficultyLabels in the anonymous
		// namespace above - the Presets tab names them too.
		constexpr const char* const kRegenKeys[] = { "CPC_Regen_HealthRate", "CPC_Regen_MagickaRate", "CPC_Regen_StaminaRate",
													 "CPC_Regen_HealthDelay", "CPC_Regen_MagickaDelay", "CPC_Regen_StaminaDelay",
													 "CPC_Regen_DamagedDelay" };
		constexpr const char* kRegenLabels[7] = { "Health regen rate in combat", "Magicka regen rate in combat", "Stamina regen rate in combat",
												  "Health regen delay after damage (s)", "Magicka regen delay after damage (s)", "Stamina regen delay after damage (s)",
												  "Damaged-attribute delay (s)" };
		constexpr const char* const kGlobalKeys[] = { "CPC_Global_HealthCeiling", "CPC_Global_MagickaCeiling", "CPC_Global_StaminaCeiling",
													  "CPC_Global_OutOfBreath", "CPC_Global_DownedEssential" };
		constexpr const char* kGlobalLabels[5] = { "Health delay ceiling (s)", "Magicka delay ceiling (s)", "Stamina delay ceiling (s)",
												   "Out-of-breath stamina delay (s)", "Downed essential regen rate" };
		static int editing = -1;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Diff_Intro", "Skyrim's own numbers per difficulty: the damage multipliers, and regeneration - which vanilla does not "
							  "vary by difficulty at all; this tab adds that. The difficulty you play on decides which set the game reads."));
		ImGuiMCP::Spacing();
		const int active = Difficulty::Current();
		if (DifficultyValues::StandingDown())
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Diff_StandingDown", "Standing down: Custom Difficulty UI is loaded and owns these values. Nothing here is applied while it is."));
		}
		if (editing < 0) { editing = active >= 0 ? active : 2; }
		ImGuiMCP::Text(strings::TR("CPC_Diff_Now", "Game difficulty now: %s"), active >= 0 ? strings::TR(kDifficultyKeys[active], kDifficultyLabels[active]) : strings::TR("CPC_Diff_NoneLoaded", "none (no character loaded)"));

		ImGuiMCP::PushItemWidth(260.0F);
		bool changed = false;

		// The built-in patch (plan section 21): the overhaul's numbers are the loaded values, and this tab
		// writes last while a control is on.
		const std::string overhaul = DifficultyValues::OverhaulLoaded();
		if (!overhaul.empty())
		{
			ImGuiMCP::TextWrapped(strings::TR("CPC_Diff_OverhaulLoaded", "%s is loaded. Its damage multipliers and regeneration values are the loaded values shown below; "
								  "nothing here touches them until a control is switched on - then this tab writes last and supersedes them."),
								  overhaul.c_str());
			bool bbFound = false;
			const bool bbScaling = DifficultyValues::BladeAndBluntLevelScaling(bbFound);
			if (overhaul.find("Blade and Blunt") != std::string::npos)
			{
				if (bbScaling) { ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Diff_BBScaling", "BladeAndBlunt.ini has bLevelBasedDifficulty = true: its DLL steps the multipliers at levels 10 to 50 as well. Set it to false while this tab controls damage - two writers on one value is never stable. The Difficulty-by-level rule below does the same job.")); }
				else if (!bbFound) { ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Diff_BBNotFound", "BladeAndBlunt.ini was not found, so its bLevelBasedDifficulty could not be read. If it is true, set it to false while this tab controls damage.")); }
			}
		}

		ImGuiMCP::SeparatorText(strings::TR("CPC_Diff_SecDamage", "Damage"));
		bool dmg = damage::control;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Diff_ControlDamage", "Control damage multipliers"), &dmg)) { damage::control = dmg; changed = true; }
		HelpMarker(strings::TR("CPC_Diff_HelpDamage", "Off writes nothing: the multipliers your game loaded with (vanilla, or an overhaul's) stay exactly as they are, "
				   "and switching off hands them back. On: the pairs below are written, and the game reads the pair for the "
				   "difficulty you play on."));
		if (damage::control)
		{
			bool shared = damage::sharedPair;
			if (ImGuiMCP::Toggle(strings::TR("CPC_Diff_SharedPair", "One pair for every difficulty"), &shared)) { damage::sharedPair = shared; changed = true; }
			HelpMarker(strings::TR("CPC_Diff_HelpSharedPair", "On: the single pair below is written for all six difficulties, so the game's difficulty setting makes no "
					   "difference to damage. Off: each difficulty has its own pair."));
			if (damage::sharedPair)
			{
				changed |= NudgeableSlider(strings::TR("CPC_Diff_DamageToYou", "Damage to you"), &damage::sharedToPlayer, 0.0F, 999.0F, "%.2f", 0.01F);
				changed |= NudgeableSlider(strings::TR("CPC_Diff_DamageByYou", "Damage by you"), &damage::sharedByPlayer, 0.0F, 999.0F, "%.2f", 0.01F);
				HelpMarker(strings::TR("CPC_Diff_HelpCtrlClick", "Ctrl+click a slider to type a value."));
			}
			else
			{
				ImGuiMCP::Text("%s", strings::TR("CPC_Diff_FillFrom", "Fill the table from:"));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(strings::TR("CPC_Diff_LoadedValues", "Loaded values"))) { DifficultyValues::UseLoadedValues(); changed = true; statusMessage = strings::TR("CPC_Diff_StatusLoaded", "The table holds the values this game loaded with."); }
				HelpMarker(strings::TR("CPC_Diff_HelpLoadedValues", "Whatever your game holds at load - vanilla, or the overhaul you run. The starting point for tuning an overhaul without losing its numbers."));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Vanilla")) { DifficultyValues::UseVanillaValues(); changed = true; statusMessage = strings::TR("CPC_Diff_StatusVanilla", "The table holds Skyrim's vanilla values."); }
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Blade and Blunt")) { DifficultyValues::UseBladeAndBlunt(); changed = true; statusMessage = strings::TR("CPC_Diff_StatusBladeAndBlunt", "The table holds Blade and Blunt's values."); }
				HelpMarker(strings::TR("CPC_Diff_HelpBladeAndBlunt", "Its published pairs: to you as vanilla, by you 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5."));
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button("Requiem")) { DifficultyValues::UseRequiem(); changed = true; statusMessage = strings::TR("CPC_Diff_StatusRequiem", "The table holds Requiem's values."); }
				HelpMarker(strings::TR("CPC_Diff_HelpRequiem", "Every multiplier 1.0 - in Requiem the difficulty setting does no damage scaling by design."));
				for (int d = 0; d < 6; ++d)
				{
					ImGuiMCP::PushID(d);
					const std::string difficultyName = strings::TR(kDifficultyKeys[d], kDifficultyLabels[d]);
					ImGuiMCP::SeparatorText(d == active ? TrFormat(strings::TR("CPC_Diff_Playing", "%s (playing)"), difficultyName.c_str()).c_str() : difficultyName.c_str());
					changed |= NudgeableSlider(strings::TR("CPC_Diff_DamageToYou", "Damage to you"), &damage::toPlayer[d], 0.0F, 999.0F, "%.2f", 0.01F);
					changed |= NudgeableSlider(strings::TR("CPC_Diff_DamageByYou", "Damage by you"), &damage::byPlayer[d], 0.0F, 999.0F, "%.2f", 0.01F);
					ImGuiMCP::TextDisabled(strings::TR("CPC_Diff_LoadedWith", "    loaded with: x%.2f to you, x%.2f by you"), DifficultyValues::LoadedDamage(d, true), DifficultyValues::LoadedDamage(d, false));
					ImGuiMCP::PopID();
				}
				HelpMarker(strings::TR("CPC_Diff_HelpVanillaPairs", "Vanilla: to you 0.5 / 0.75 / 1 / 1.5 / 2 / 3, by you 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25, Novice to Legendary. Ctrl+click a slider to type a value."));
			}
		}

		ImGuiMCP::SeparatorText(strings::TR("CPC_Diff_SecByLevel", "Difficulty by level"));
		bool byLevel = bylevel::enabled;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Diff_ByLevel", "Set the game's difficulty from your level"), &byLevel))
		{
			bylevel::enabled = byLevel;
			changed = true;
			if (byLevel) { DifficultyValues::ApplyLevelRule("switched on"); }
		}
		HelpMarker(strings::TR("CPC_Diff_HelpByLevel", "On a save load and on every level-up, the highest difficulty whose level you have reached becomes the game's "
				   "difficulty - the same change the Settings menu makes, so everything that follows difficulty follows it. "
				   "0 = that difficulty is never chosen by this rule. Off: the game's difficulty is yours to set."));
		if (bylevel::enabled)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			const int level = player ? static_cast<int>(player->GetLevel()) : -1;
			const int target = level >= 0 ? DifficultyValues::LevelRuleTarget(level) : -1;
			if (level >= 0) { ImGuiMCP::Text(strings::TR("CPC_Diff_LevelMapsTo", "Level %d -> %s"), level, target >= 0 ? strings::TR(kDifficultyKeys[target], kDifficultyLabels[target]) : strings::TR("CPC_Diff_NoRowApplies", "no row applies")); }
			for (int d = 0; d < 6; ++d)
			{
				ImGuiMCP::PushID(100 + d);
				int from = static_cast<int>(bylevel::levelFor[d]);
				if (ImGuiMCP::InputInt(TrFormat(strings::TR("CPC_Diff_FromLevel", "%s from level"), strings::TR(kDifficultyKeys[d], kDifficultyLabels[d])).c_str(), &from)) { bylevel::levelFor[d] = static_cast<std::uint32_t>(std::clamp(from, 0, 1000)); changed = true; }
				ImGuiMCP::PopID();
			}
			HelpMarker(strings::TR("CPC_Diff_HelpMilestones", "Defaults are Blade and Blunt's milestones: one difficulty tier per ten levels."));
		}

		ImGuiMCP::SeparatorText(strings::TR("CPC_Diff_SecRegen", "Regeneration"));
		bool rg = regen::control;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Diff_ControlRegen", "Control regeneration"), &rg)) { regen::control = rg; changed = true; }
		HelpMarker(strings::TR("CPC_Diff_HelpRegen", "Vanilla has one set of regeneration values for every difficulty. Here each difficulty has its own set, "
				   "and the set for the difficulty you play on is what the game reads - switched the moment you change "
				   "difficulty in the game's Settings. Off restores the values your game came with."));
		if (regen::control)
		{
			ComboTR(strings::TR("CPC_Diff_Editing", "Editing"), &editing, kDifficultyKeys, kDifficultyLabels, 6);
			HelpMarker(strings::TR("CPC_Diff_HelpEditing", "Which difficulty's set the sliders below show. The game reads the set for the difficulty you play on."));
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("CPC_Diff_CopyToAll", "Copy to all difficulties"))) { DifficultyValues::CopyRegenToAll(editing); changed = true; }
			HelpMarker(strings::TR("CPC_Diff_HelpCopyToAll", "Every other difficulty's set becomes a copy of this one."));
			for (int i = 0; i < 7; ++i)
			{
				if (!DifficultyValues::RegenResolved(i)) { ImGuiMCP::TextDisabled(strings::TR("CPC_Diff_NotInSettings", "%s - not in this game's settings"), strings::TR(kRegenKeys[i], kRegenLabels[i])); continue; }
				const bool delay = i >= 3;
				changed |= NudgeableSlider(strings::TR(kRegenKeys[i], kRegenLabels[i]), &regen::perDifficulty[i][editing], 0.0F, delay ? 30.0F : 5.0F, "%.2f", delay ? 0.5F : 0.05F);
				HelpMarker(TrFormat(strings::TR("CPC_Diff_HelpRegenValue", "%s - your game's own value is %.2f."), DifficultyValues::RegenSettingName(i), DifficultyValues::RegenVanilla(i)).c_str());
			}
			ImGuiMCP::SeparatorText(strings::TR("CPC_Diff_SecEveryDifficulty", "Every difficulty"));
			for (int i = 0; i < 5; ++i)
			{
				if (!DifficultyValues::GlobalResolved(i)) { ImGuiMCP::TextDisabled(strings::TR("CPC_Diff_NotInSettings", "%s - not in this game's settings"), strings::TR(kGlobalKeys[i], kGlobalLabels[i])); continue; }
				const bool rate = i == 4;
				changed |= NudgeableSlider(strings::TR(kGlobalKeys[i], kGlobalLabels[i]), &regen::global[i], 0.0F, rate ? 5.0F : 60.0F, "%.2f", rate ? 0.05F : 0.5F);
				HelpMarker(TrFormat(strings::TR("CPC_Diff_HelpGlobalValue", "%s - one value for every difficulty; your game's own value is %.2f."), DifficultyValues::GlobalSettingName(i), DifficultyValues::GlobalVanilla(i)).c_str());
			}
		}
		if (changed) { DifficultyValues::RequestApply(); }
		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Diff_SecUsingNow", "What the game is using right now"));
		if (active >= 0)
		{
			ImGuiMCP::Text(strings::TR("CPC_Diff_DamageLine", "Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)"), strings::TR(kDifficultyKeys[active], kDifficultyLabels[active]),
						   DifficultyValues::LiveDamage(active, true), DifficultyValues::LiveDamage(active, false),
						   DifficultyValues::LoadedDamage(active, true), DifficultyValues::LoadedDamage(active, false));
		}
		ImGuiMCP::Text(strings::TR("CPC_Diff_RegenLine", "Regeneration in combat: health x%.2f, magicka x%.2f, stamina x%.2f"), DifficultyValues::LiveRegen(0), DifficultyValues::LiveRegen(1), DifficultyValues::LiveRegen(2));
		ImGuiMCP::Text(strings::TR("CPC_Diff_DelayLine", "Delay after damage: health %.1f s, magicka %.1f s, stamina %.1f s"), DifficultyValues::LiveRegen(3), DifficultyValues::LiveRegen(4), DifficultyValues::LiveRegen(5));
		if (!damage::control && !regen::control) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Diff_NotControlling", "Not controlling either - nothing is written; the values above are whatever the game loaded with.")); }
		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button(strings::TR("CPC_Btn_ApplyNow", "Apply now"))) { DifficultyValues::RequestApply(); statusMessage = strings::TR("CPC_Status_Applied", "Applied."); }
		HelpMarker(strings::TR("CPC_Diff_HelpApply", "Re-writes the values. It also runs by itself when a save loads, when the game's difficulty changes, and when you change anything above."));

		RenderButtons();
	}

	void __stdcall ExperiencePanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Exp_Intro", "Character experience from what you do: quests completed, places discovered, dungeons cleared, kills "
							  "and books read - alongside skill use, or instead of it. Every amount is this mod's own number; tune it "
							  "against the Levelling tab's level cost."));
		ImGuiMCP::Spacing();
		if (ExperienceSources::StandingDown())
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Exp_StandingDown", "Standing down: the Experience mod is loaded and owns where experience comes from. Nothing here is granted while it is."));
		}

		ImGuiMCP::PushItemWidth(260.0F);
		bool on = experience::enabled;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Exp_Enabled", "Earn experience from quests, exploration and kills"), &on)) { experience::enabled = on; }
		HelpMarker(strings::TR("CPC_Exp_HelpEnabled", "Off by default. While it is off nothing is granted and the game levels exactly as before."));
		if (experience::enabled)
		{
			bool pay = experience::skillsPay;
			if (ImGuiMCP::Toggle(strings::TR("CPC_Exp_SkillsPay", "Skill increases still pay toward your level"), &pay)) { experience::skillsPay = pay; }
			HelpMarker(strings::TR("CPC_Exp_HelpSkillsPay", "On: the sources below add to what skill use already pays (supplement). Off: skill increases pay nothing "
					   "toward your level and the sources below are the only way to level (replace)."));
			if (!experience::skillsPay && !Patches::IsInstalled("Skill-to-level income"))
			{
				ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Exp_RestartNeeded", "Restart the game for skill increases to stop paying: the level-income patch attaches at startup and was not needed when this session started."));
			}

			ImGuiMCP::SeparatorText(strings::TR("CPC_Exp_SecQuests", "Quests completed, by kind"));
			NudgeableSlider(strings::TR("CPC_Exp_QuestMain", "Main quest"), &experience::questMain, 0.0F, 2000.0F, "%.0f", 10.0F);
			NudgeableSlider(strings::TR("CPC_Exp_QuestFaction", "Faction quest"), &experience::questFaction, 0.0F, 2000.0F, "%.0f", 10.0F);
			HelpMarker(strings::TR("CPC_Exp_HelpFaction", "The guilds, the Companions, the Dark Brotherhood, the civil war, and the Dawnguard and Dragonborn lines."));
			NudgeableSlider(strings::TR("CPC_Exp_QuestDaedric", "Daedric quest"), &experience::questDaedric, 0.0F, 2000.0F, "%.0f", 10.0F);
			NudgeableSlider(strings::TR("CPC_Exp_QuestSide", "Side quest"), &experience::questSide, 0.0F, 2000.0F, "%.0f", 10.0F);
			NudgeableSlider(strings::TR("CPC_Exp_QuestMisc", "Miscellaneous objective"), &experience::questMisc, 0.0F, 500.0F, "%.0f", 5.0F);
			NudgeableSlider(strings::TR("CPC_Exp_QuestOther", "Other quest"), &experience::questOther, 0.0F, 2000.0F, "%.0f", 10.0F);
			HelpMarker(strings::TR("CPC_Exp_HelpOther", "Quests the game gives no kind - many mod-added ones."));

			ImGuiMCP::SeparatorText(strings::TR("CPC_Exp_SecExploration", "Exploration"));
			NudgeableSlider(strings::TR("CPC_Exp_Location", "Location discovered"), &experience::location, 0.0F, 500.0F, "%.0f", 5.0F);
			NudgeableSlider(strings::TR("CPC_Exp_Cleared", "Location cleared"), &experience::cleared, 0.0F, 1000.0F, "%.0f", 10.0F);

			ImGuiMCP::SeparatorText(strings::TR("CPC_Exp_SecKills", "Kills"));
			NudgeableSlider(strings::TR("CPC_Exp_KillBase", "Per kill"), &experience::killBase, 0.0F, 200.0F, "%.1f", 1.0F);
			NudgeableSlider(strings::TR("CPC_Exp_KillPerLevel", "Plus, per level of the victim"), &experience::killPerLevel, 0.0F, 20.0F, "%.1f", 0.5F);
			bool fk = experience::followerKills;
			if (ImGuiMCP::Toggle(strings::TR("CPC_Exp_FollowerKills", "Count kills by followers and summons"), &fk)) { experience::followerKills = fk; }

			ImGuiMCP::SeparatorText(strings::TR("CPC_Exp_SecBooks", "Books"));
			NudgeableSlider(strings::TR("CPC_Exp_Book", "Book read"), &experience::book, 0.0F, 200.0F, "%.0f", 1.0F);
			NudgeableSlider(strings::TR("CPC_Exp_SkillBook", "Skill book read"), &experience::skillBook, 0.0F, 500.0F, "%.0f", 5.0F);
		}
		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Sec_RightNow", "What that means right now"));
		{
			const auto st = ExperienceSources::GetState();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* skills = player ? player->GetInfoRuntimeData().skills : nullptr;
			if (skills && skills->data)
			{
				ImGuiMCP::Text(strings::TR("CPC_Exp_LevelLine", "Level %u: %.0f of %.0f experience toward the next"), static_cast<unsigned>(player->GetLevel()), skills->data->xp, skills->data->levelThreshold);
			}
			ImGuiMCP::Text(strings::TR("CPC_Exp_CountsLine", "This character, since the count began: %u quests (%.0f), %u locations (%.0f), %u cleared (%.0f), %u kills (%.0f), %u books (%.0f)"),
						   st.count[0], st.xp[0], st.count[1], st.xp[1], st.count[2], st.xp[2], st.count[3], st.xp[3], st.count[4], st.xp[4]);
			if (!st.last.empty()) { ImGuiMCP::Text(strings::TR("CPC_Exp_Last", "Last: %s"), st.last.c_str()); }
			if (!experience::enabled) { ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Exp_NotGranting", "Not granting anything - experience comes from skill use, as in vanilla.")); }
		}

		RenderButtons();
	}

	void __stdcall StaticLevellingPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Stat_Intro", "A fixed amount of experience per use of a skill, instead of vanilla's "
							  "scaling - so a skill advances at the same rate at level 5 and level 50."));
		ImGuiMCP::Spacing();
		InertNotice("Static levelling", strings::TR("CPC_Stat_Inert", "skill experience still scales the vanilla way"));

		ImGuiMCP::PushItemWidth(200.0F);

		bool on = staticlevel::enabled;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Stat_Enabled", "Use static skill levelling"), &on)) { staticlevel::enabled = on; }
		HelpMarker(strings::TR("CPC_Stat_HelpEnabled", "Off by default. While it is off, nothing about skill experience is asserted."));

		if (staticlevel::enabled)
		{
			ImGuiMCP::Spacing();
			ImGuiMCP::SeparatorText(strings::TR("CPC_Stat_SecPerUse", "Experience per use, as a percent of a skill level"));
			HelpMarker(strings::TR("CPC_Stat_HelpPerUse", "1.0 = one use is one percent of the level, so 100 uses raise the skill by one level whether it is at 5 or at 50. "
					   "Perk bonuses to skill use are not applied while this is on - the amount stays fixed."));
			for (int i = 0; i < skilllist::kCount; ++i)
			{
				ImGuiMCP::PushID(skilllist::kIniName[i]);
				NudgeableSlider(strings::TR(kSkillKeys[i], kSkillLabels[i]), &staticlevel::xpPerUse[i], 0.0F, 100.0F, "%.2f", 0.25F);
				ImGuiMCP::PopID();
			}
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Stat_SecPoints", "Skill points"));
		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Stat_PointsIntro", "Skills advance only by points spent in the level-up menu: each level grants points, the menu "
							  "shows every skill with + and -, and the choice applies when you pick your attribute. Needs the "
							  "skill-point level-up menu file (this mod ships one; Static Skill Leveling Rewritten's skins fit too)."));
		bool pts = staticlevel::pointsEnabled;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Stat_UsePoints", "Use skill points"), &pts)) { staticlevel::pointsEnabled = pts; }
		HelpMarker(strings::TR("CPC_Stat_HelpUsePoints", "Off by default. While on, ordinary skill experience is not banked - use only counts through points - and a "
				   "point-spent level pays nothing toward the character level. Takes effect after a restart."));
		if (staticlevel::pointsEnabled)
		{
			if (ImGuiMCP::InputInt(strings::TR("CPC_Stat_PointsPerLevel", "Points per level"), &staticlevel::pointsPerLevel)) { staticlevel::pointsPerLevel = std::clamp(staticlevel::pointsPerLevel, 0, 500); }
			NudgeableSlider(strings::TR("CPC_Stat_PointsLevelMult", "Plus, per character level"), &staticlevel::pointsLevelMult, -10.0F, 20.0F, "%.1f", 0.5F);
			HelpMarker(strings::TR("CPC_Stat_HelpPointsMult", "Points granted at a level = points per level + this x the level, whole numbers. Negative slows the curve."));
			if (ImGuiMCP::InputInt(strings::TR("CPC_Stat_BankCap", "Bank cap (0 = none)"), &staticlevel::pointsCap)) { staticlevel::pointsCap = std::clamp(staticlevel::pointsCap, 0, 100000); }
			if (ImGuiMCP::InputInt(strings::TR("CPC_Stat_MaxIncreases", "Most increases per skill, per level up"), &staticlevel::maxIncreasesPerSkill)) { staticlevel::maxIncreasesPerSkill = std::clamp(staticlevel::maxIncreasesPerSkill, 1, 100); }
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Stat_CostIntro", "Cost of one skill level, by the skill's current level:"));
			constexpr const char* const kTierKeys[] = { "CPC_Stat_TierBelow25", "CPC_Stat_Tier25", "CPC_Stat_Tier50", "CPC_Stat_Tier75" };
			constexpr const char* const kTierLabels[] = { "Below 25", "25 to 49", "50 to 74", "75 and up" };
			for (int i = 0; i < 4; ++i) { if (ImGuiMCP::InputInt(strings::TR(kTierKeys[i], kTierLabels[i]), &staticlevel::cost[i])) { staticlevel::cost[i] = std::clamp(staticlevel::cost[i], 1, 1000); } }
			if (auto* player = RE::PlayerCharacter::GetSingleton())
			{
				ImGuiMCP::Text(strings::TR("CPC_Stat_BankLine", "This character: %d point(s) banked; the next level grants %d."), SkillPoints::Bank(), SkillPoints::PointsForLevel(static_cast<std::uint16_t>(player->GetLevel() + 1)));
			}
			ImGuiMCP::TextWrapped("%s", SkillPoints::MenuStatus().c_str());
		}

		ImGuiMCP::PopItemWidth();
		RenderButtons();
	}

	void __stdcall PresetsPanel::Render()
	{
		strings::Tick();

		static char newName[64] = "My preset";
		static bool scanned = false;
		if (!scanned) { Presets::Refresh(); scanned = true; }

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Pre_Intro", "A preset is a file in this mod's Presets folder, in the same format as its "
							  "INI. Whichever one is selected is what the mod is using - and the selection "
							  "belongs to THIS character, while the files are shared, so two saves can sit "
							  "on different presets at once."));
		ImGuiMCP::Spacing();

		const auto& all = Presets::All();
		ImGuiMCP::Text(strings::TR("CPC_Pre_Using", "This character is using: %s"), Presets::Current().c_str());
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText(strings::TR("CPC_Pre_SecDifficulty", "Game difficulty"));
		bool follow = settings::difficulty::follow;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Pre_Follow", "Follow the game's difficulty"), &follow))
		{
			settings::difficulty::follow = follow;
			OnMainThread([]() { statusMessage = Difficulty::OnFollowChanged(); });
		}
		HelpMarker(strings::TR("CPC_Pre_HelpFollow", "One configuration per difficulty. While this is on, the difficulty set in the game's own "
				   "Settings decides which of six presets is in use - Difficulty - Novice, Apprentice, Adept, Expert, "
				   "Master, Legendary - and changing it switches: what you had is saved into the old difficulty's "
				   "preset and the new one's is loaded (made from the current configuration the first time). "
				   "Saved with the mod's INI, so it stays on between sessions."));
		const int gameDifficulty = Difficulty::Current();
		const std::string difficultyName = gameDifficulty >= 0
											 ? strings::TR(kDifficultyKeys[gameDifficulty], kDifficultyLabels[gameDifficulty])
											 : strings::TR("CPC_Difficulty_NoneShort", "none");
		// The preset a difficulty maps to is a FILE name - never translated, only quoted.
		const std::string presetSuffix = settings::difficulty::follow
											 ? TrFormat(strings::TR("CPC_Pre_ArrowPreset", " -> preset \"%s\""), Difficulty::PresetNameFor(gameDifficulty).c_str())
											 : std::string{};
		ImGuiMCP::Text(strings::TR("CPC_Pre_GameIsOn", "The game is on %s%s"), difficultyName.c_str(),
					   presetSuffix.c_str());
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText(strings::TR("CPC_Pre_SecPresets", "Presets"));
		for (const auto& name : all)
		{
			ImGuiMCP::PushID(name.c_str());
			const bool isCurrent = (name == Presets::Current());
			if (ImGuiMCP::Button(isCurrent ? strings::TR("CPC_Pre_InUse", "In use") : strings::TR("CPC_Pre_Use", "Use")))
			{
				const std::string picked = name;
				OnMainThread([picked]() {
					statusMessage = Presets::Select(picked)
										? TrFormat(strings::TR("CPC_Pre_NowUsing", "Now using %s."), picked.c_str())
										: std::string(strings::TR("CPC_Pre_SelectFailed", "Could not read that preset - fell back to the built-in default."));
				});
			}
			ImGuiMCP::SameLine();
			ImGuiMCP::Text("%s", name.c_str());
			ImGuiMCP::PopID();
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Pre_SecThisConfig", "This configuration"));

		if (ImGuiMCP::Button(strings::TR("CPC_Pre_SaveInto", "Save into the selected preset")))
		{
			OnMainThread([]() {
				statusMessage = Presets::SaveCurrent()
									? TrFormat(strings::TR("CPC_Pre_SavedInto", "Saved into %s."), Presets::Current().c_str())
									: std::string(strings::TR("CPC_Pre_SaveIntoFailed", "The built-in default is not a file - use Save as a new preset instead."));
			});
		}
		HelpMarker(strings::TR("CPC_Pre_HelpSaveInto", "A preset is a living configuration, not a read-only template: this writes what is on "
				   "the pages now back into the preset you are using."));

		ImGuiMCP::PushItemWidth(200.0F);
		ImGuiMCP::InputText(strings::TR("CPC_Pre_NewName", "New preset name"), newName, sizeof(newName));
		ImGuiMCP::PopItemWidth();
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button(strings::TR("CPC_Pre_SaveAsNew", "Save as a new preset")))
		{
			const std::string wanted = newName;
			OnMainThread([wanted]() {
				statusMessage = Presets::Export(wanted)
									? TrFormat(strings::TR("CPC_Pre_WroteAndSelected", "Wrote and selected %s."), wanted.c_str())
									: std::string(strings::TR("CPC_Pre_BadName", "That name cannot be used as a file name."));
			});
		}
		HelpMarker(strings::TR("CPC_Pre_HelpSaveAsNew", "Writes the current configuration out as its own file, which is also how you share one."));

		if (Presets::Current() != Presets::kDefaultName)
		{
			if (ImGuiMCP::Button(strings::TR("CPC_Pre_Delete", "Delete the selected preset")))
			{
				const std::string doomed = Presets::Current();
				OnMainThread([doomed]() {
					statusMessage = Presets::Delete(doomed)
										? TrFormat(strings::TR("CPC_Pre_Deleted", "Deleted %s; back on the built-in default."), doomed.c_str())
										: std::string(strings::TR("CPC_Pre_DeleteFailed", "Could not delete that preset."));
				});
			}
			HelpMarker(strings::TR("CPC_Pre_HelpDelete", "Deletes the file. The built-in default can never be deleted, so there is always "
					   "somewhere to fall back to."));
		}

		ImGuiMCP::Spacing();
		ImGuiMCP::Text("%s", Presets::Dir().c_str());

		RenderButtons();
	}

	void __stdcall EnchantingPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		const auto& group = Enchanting::Settings();

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Ench_Intro", "How the cost of using an enchanted item scales with your Enchanting skill. "
							  "This is the equation that breaks when Enchanting is uncapped, which is why it "
							  "sits beside the skill caps."));
		ImGuiMCP::Spacing();

		if (!group.Captured() || group.FoundCount() == 0)
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Ench_NotCaptured", "None of the enchanting cost settings could be read on this runtime, so "
								  "this tab will not write anything. See the log."));
			RenderButtons();
			return;
		}
		if (group.FoundCount() < static_cast<int>(group.Entries().size()))
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Ench_Partial", "Only some of these settings exist on this runtime; the rest are left alone."));
			ImGuiMCP::Spacing();
		}

		ImGuiMCP::PushItemWidth(260.0F);

		bool changed = false;
		bool over = enchanting::overrideCost;
		if (ImGuiMCP::Toggle(strings::TR("CPC_Ench_Control", "Control the enchantment charge cost"), &over))
		{
			enchanting::overrideCost = over;
			changed = true;
		}
		HelpMarker(strings::TR("CPC_Ench_HelpControl", "Off by default. While it is off this mod restores the values your install came with "
				   "and leaves them alone."));

		if (enchanting::overrideCost)
		{
			changed |= NudgeableSlider(strings::TR("CPC_Ench_Base", "Cost base"), &enchanting::costBase, 0.0F, 100.0F, "%.2f", 0.5F);
			changed |= NudgeableSlider(strings::TR("CPC_Ench_Scale", "Cost scale"), &enchanting::costScale, 0.0F, 10.0F, "%.2f", 0.05F);
			changed |= NudgeableSlider(strings::TR("CPC_Ench_Mult", "Cost multiplier"), &enchanting::costMult, 0.0F, 100.0F, "%.2f", 0.5F);
			changed |= NudgeableSlider(strings::TR("CPC_Ench_Exponent", "Cost exponent"), &enchanting::costExponent, 0.0F, 5.0F, "%.2f", 0.05F);
			HelpMarker(strings::TR("CPC_Ench_HelpValues", "Lower values make an enchanted item cheaper to use. The exponent is the one that "
					   "runs away when the skill climbs past 100."));
		}

		if (changed) { Enchanting::RequestApply(); }

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Ench_SecUsing", "What the game is using"));
		const auto& entries = group.Entries();
		for (int i = 0; i < static_cast<int>(entries.size()); ++i)
		{
			if (!entries[static_cast<std::size_t>(i)].found)
			{
				ImGuiMCP::TextDisabled(strings::TR("CPC_Ench_NotOnRuntime", "%s - not on this runtime"), entries[static_cast<std::size_t>(i)].name);
				continue;
			}
			ImGuiMCP::Text(strings::TR("CPC_Ench_ValueLine", "%s = %.3f   (this install: %.3f)"), entries[static_cast<std::size_t>(i)].name,
						   group.Live(i), entries[static_cast<std::size_t>(i)].vanilla);
		}

		RenderButtons();
	}

	void __stdcall PatchesPanel::Render()
	{
		strings::Tick();

		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Pat_Intro", "Each engine patch this mod installs, and what it changes. A patch that is "
							  "not active is not a fault - that part of the game simply behaves as it "
							  "would without this mod."));
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText(strings::TR("CPC_Pat_SecWhatElse", "What else is installed"));
		ImGuiMCP::TextWrapped("%s", strings::TR("CPC_Pat_DetectedIntro", "Detected at load. A conflict found here switches OUR feature off - "
							  "never another mod's - and every switch stays yours to override."));
		ImGuiMCP::Spacing();
		for (const auto& d : Compat::All())
		{
			if (d.present) { ImGuiMCP::Text(strings::TR("CPC_Pat_Installed", "%s: installed"), d.name.c_str()); }
			else { ImGuiMCP::TextDisabled(strings::TR("CPC_Pat_NotInstalled", "%s: not installed"), d.name.c_str()); }
			ImGuiMCP::TextWrapped("    %s", d.consequence.c_str());
		}
		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("CPC_Pat_SecEnginePatches", "Engine patches"));

		const auto& groups = Patches::All();
		if (groups.empty())
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("CPC_Pat_NoGroups", "No patch groups are registered in this build."));
			RenderButtons();
			return;
		}

		for (const auto& g : groups)
		{
			ImGuiMCP::SeparatorText(g.name.c_str());
			ImGuiMCP::Text("%s", g.installed ? strings::TR("CPC_Pat_Active", "Active") : strings::TR("CPC_Pat_NotActive", "Not active"));
			ImGuiMCP::TextWrapped(strings::TR("CPC_Pat_Touches", "Touches: %s"), g.touches.c_str());
			if (!g.installed)
			{
				ImGuiMCP::TextWrapped(strings::TR("CPC_Pat_Why", "Why: %s"), g.status.c_str());
			}
			ImGuiMCP::Spacing();
		}

		RenderButtons();
	}

	void __stdcall DebugPanel::Render()
	{
		strings::Tick();

		using namespace settings;

		ImGuiMCP::SeparatorText(strings::TR("CPC_Dbg_Sec", "Debug"));

		ImGuiMCP::PushItemWidth(260.0F);

		int level = static_cast<int>(debug::logLevel);
		level = std::clamp(level, 0, kLogLevelCount - 1);
		if (ComboTR(strings::TR("CPC_Dbg_LogLevel", "Log level"), &level, kLogLevelKeys, kLogLevelLabels, kLogLevelCount))
		{
			debug::logLevel = static_cast<std::uint32_t>(level);
			ApplyLogLevel();
		}
		HelpMarker(strings::TR("CPC_Dbg_HelpLogLevel", "Applies immediately. The log is at Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log."));

		ImGuiMCP::PopItemWidth();

		ImGuiMCP::Spacing();
		const auto s = Levelling::GetState();
		ImGuiMCP::Text(strings::TR("CPC_Dbg_Applications", "Level cost applied %llu time(s) this session"), static_cast<unsigned long long>(s.applications));
		ImGuiMCP::Text(strings::TR("CPC_Dbg_LiveSettings", "Live game settings: base %.1f, per level %.1f"), s.liveBase, s.liveMult);

		RenderButtons();
	}
}
