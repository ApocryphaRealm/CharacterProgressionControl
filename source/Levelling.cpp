#include "PCH.h"

#include "Levelling.h"

#include "Settings.h"

#include "utils/Logger.h"

#include <atomic>

namespace Levelling
{
	namespace
	{
		constexpr const char* kBaseSetting = "fXPLevelUpBase";
		constexpr const char* kMultSetting = "fXPLevelUpMult";

		bool captured = false;
		float vanillaBase = 0.0F;
		float vanillaMult = 0.0F;
		std::atomic<std::uint64_t> applications{ 0 };

		RE::Setting* Find(const char* a_name)
		{
			auto* collection = RE::GameSettingCollection::GetSingleton();
			return collection ? collection->GetSetting(a_name) : nullptr;
		}

		bool ReadSetting(const char* a_name, float& a_out)
		{
			if (auto* setting = Find(a_name))
			{
				a_out = setting->GetFloat();
				return true;
			}
			logger::warn("game setting {} not found", a_name);
			return false;
		}

		bool WriteSetting(const char* a_name, float a_value)
		{
			if (auto* setting = Find(a_name))
			{
				setting->data.f = a_value;
				return true;
			}
			logger::warn("game setting {} not found; nothing written", a_name);
			return false;
		}

		std::uint16_t PlayerLevel()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player ? player->GetLevel() : static_cast<std::uint16_t>(0);
		}
	}

	float CostForLevel(float a_base, float a_mult, std::uint16_t a_level)
	{
		return a_base + a_mult * static_cast<float>(a_level);
	}

	void CaptureVanilla()
	{
		if (captured) { return; }

		const bool gotBase = ReadSetting(kBaseSetting, vanillaBase);
		const bool gotMult = ReadSetting(kMultSetting, vanillaMult);
		captured = gotBase && gotMult;
		if (!captured)
		{
			logger::error("could not read the level-cost game settings; the Levelling tab will not write anything");
			return;
		}

		logger::info("captured this install's level cost: {} = {:.1f}, {} = {:.1f}",
					 kBaseSetting, vanillaBase, kMultSetting, vanillaMult);

		// First run seeds the page from what this install actually has, so opening the tab shows
		// the truth rather than a number remembered from some other copy of the game.
		if (!settings::levelling::seeded)
		{
			settings::levelling::base = vanillaBase;
			settings::levelling::mult = vanillaMult;
			settings::levelling::seeded = true;
			logger::debug("seeded the Levelling tab from this install's own values");
		}
		settings::SetCapturedDefaults(vanillaBase, vanillaMult);
	}

	void Apply()
	{
		if (!captured)
		{
			logger::debug("Apply skipped - the game's own values were never captured");
			return;
		}

		// Override off: put back what the game had and then keep our hands off it. Anything
		// another mod sets afterwards is that mod's business.
		const float base = settings::levelling::overrideCost ? settings::levelling::base : vanillaBase;
		const float mult = settings::levelling::overrideCost ? settings::levelling::mult : vanillaMult;

		WriteSetting(kBaseSetting, base);
		WriteSetting(kMultSetting, mult);
		applications.fetch_add(1);

		logger::debug("level cost {}: {} = {:.1f}, {} = {:.1f}",
					  settings::levelling::overrideCost ? "set" : "restored to this install's own values",
					  kBaseSetting, base, kMultSetting, mult);
	}

	void RequestApply()
	{
		if (auto* task = SKSE::GetTaskInterface())
		{
			task->AddTask([]() { Apply(); });
		}
	}

	State GetState()
	{
		State s;
		s.captured = captured;
		s.overriding = settings::levelling::overrideCost;
		s.vanillaBase = vanillaBase;
		s.vanillaMult = vanillaMult;
		ReadSetting(kBaseSetting, s.liveBase);
		ReadSetting(kMultSetting, s.liveMult);
		s.playerLevel = PlayerLevel();
		s.costThisLevel = CostForLevel(s.liveBase, s.liveMult, s.playerLevel);
		s.applications = applications.load();
		return s;
	}
}
