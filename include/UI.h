#pragma once

// Character Progression Control - the settings pages. One section in the menu framework with a
// tab per area of progression; stage 1 ships Levelling and Debug, and the rest of the tabs from
// the plan are added as their engines land.

namespace UI
{
	void Register();

	namespace LevellingPanel
	{
		void __stdcall Render();
	}

	namespace ExperiencePanel
	{
		void __stdcall Render();
	}

	namespace SkillsPanel
	{
		void __stdcall Render();
	}

	namespace LevelUpPanel
	{
		void __stdcall Render();
	}

	namespace AttributesPanel
	{
		void __stdcall Render();
	}

	namespace CarryWeightPanel
	{
		void __stdcall Render();
	}

	namespace DifficultyPanel
	{
		void __stdcall Render();
	}

	namespace StaticLevellingPanel
	{
		void __stdcall Render();
	}

	namespace PresetsPanel
	{
		void __stdcall Render();
	}

	namespace EnchantingPanel
	{
		void __stdcall Render();
	}

	namespace PatchesPanel
	{
		void __stdcall Render();
	}

	namespace DebugPanel
	{
		void __stdcall Render();
	}
}
