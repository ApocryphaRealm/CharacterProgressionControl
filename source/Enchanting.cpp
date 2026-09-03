#include "PCH.h"

#include "Enchanting.h"

#include "Settings.h"

#include "utils/Logger.h"

namespace Enchanting
{
	namespace
	{
		GameSettings::Group group{ "Enchanting" };
		bool built = false;

		void Build()
		{
			if (built) { return; }
			built = true;
			// The four settings the charge-cost equation is scaled by. Any that a given runtime
			// does not have are reported and left alone rather than assumed - see GameSettings.
			group.Add("fEnchantingSkillCostBase", &settings::enchanting::costBase);
			group.Add("fEnchantingSkillCostScale", &settings::enchanting::costScale);
			group.Add("fEnchantingSkillCostMult", &settings::enchanting::costMult);
			group.Add("fEnchantingCostExponent", &settings::enchanting::costExponent);
		}
	}

	void CaptureVanilla()
	{
		Build();
		group.Capture();

		// "Restore defaults" must land on what this install had, not on a compiled-in number.
		const auto& e = group.Entries();
		if (e.size() == 4)
		{
			settings::SetCapturedEnchanting(e[0].vanilla, e[1].vanilla, e[2].vanilla, e[3].vanilla);
		}

		// First run shows this install's own numbers rather than a guess.
		if (!settings::enchanting::seeded)
		{
			group.SeedFromVanilla();
			settings::enchanting::seeded = true;
			logger::debug("seeded the Enchanting tab from this install's own values");
		}
	}

	void Apply()
	{
		Build();
		group.Apply(settings::enchanting::overrideCost);
	}

	void RequestApply()
	{
		if (auto* task = SKSE::GetTaskInterface())
		{
			task->AddTask([]() { Apply(); });
		}
	}

	const GameSettings::Group& Settings()
	{
		Build();
		return group;
	}
}
