#include "PCH.h"

#include "Skills.h"

#include "Patches.h"
#include "Settings.h"

#include "utils/Logger.h"

namespace Skills
{
	namespace
	{
		bool InstallCapPatch(std::string& a_reason)
		{
			// Raising a skill's cap means changing where the game stops advancing a skill, and
			// the formula cap means changing the value its own calculations read. Both are engine
			// patches, and an engine patch needs an Address Library ID that has been VERIFIED
			// against the runtime - a guessed one does not fail safely, it corrupts or crashes.
			//
			// CommonLibSSE-NG does not declare these functions, so the IDs are not ours to borrow
			// yet. Until they are found and checked in game, this group reports that it is off
			// and the caps behave exactly as vanilla.
			a_reason = "not implemented yet - the skill cap needs an engine patch, and its address "
					   "has not been verified against the runtime. Skills cap at 100 as in vanilla.";
			return false;
		}
	}

	void Register()
	{
		Patches::Register(
			"Skill caps",
			"Where a skill stops advancing, and the value the game's own formulas read for it.",
			InstallCapPatch);
	}

	State GetState()
	{
		State s;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return s; }

		s.characterLevel = player->GetLevel();

		// PlayerSkills is a pointer into the save's progression data; it does not exist before a
		// game is loaded, so every step here is checked rather than assumed.
		auto* skills = player->skills;
		if (!skills || !skills->data) { return s; }

		const auto* data = skills->data;
		s.characterXp = data->xp;
		s.characterThreshold = data->levelThreshold;

		for (int i = 0; i < skilllist::kCount; ++i)
		{
			s.skill[i].level = data->skills[i].level;
			s.skill[i].xp = data->skills[i].xp;
			s.skill[i].levelThreshold = data->skills[i].levelThreshold;
		}
		s.readable = true;
		return s;
	}
}
