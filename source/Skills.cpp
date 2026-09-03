#include "PCH.h"

#include "Skills.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

namespace Skills
{
	namespace
	{
		// Where the game compares a skill against its cap. The signature is the one the original
		// SE uncapper used; it is kept as DATA because it is the single thing that has to change
		// when a game update moves this code, and nothing else in the mod needs to know.
		//
		// It is NOT known to match 1.5.97: it dates from 2017, and it cannot be checked ahead of
		// time because the shipped SkyrimSE.exe is Steam-packed and its .text is encrypted on disk
		// (measured 2026-09-03 - see Signature.h). So the scan happens at load, in memory, and
		// whatever it finds is reported rather than assumed.
		constexpr const char* kSkillCapSignature =
			"48 81 C1 B0 00 00 00 41 0F 29 73 D8 45 0F 29 43 B8 48 8B 01 FF 50 18 "
			"F3 44 0F 10 ?? ?? ?? ?? 00 0F 28 F0 41 0F 2F F0 0F 83 74 02 00 00 48 8D 44 24 3C";
		constexpr std::ptrdiff_t kSkillCapOffset = 0x17;

		Signature::Result capSite;

		bool InstallCapPatch(std::string& a_reason)
		{
			// Step one, which is real: locate the site. An address found once in the running
			// image is a verified address for whatever build this is.
			capSite = Signature::Find(kSkillCapSignature, kSkillCapOffset);
			if (!capSite.found)
			{
				a_reason = capSite.note + ". Skills cap at 100 as in vanilla.";
				return false;
			}

			// Step two, which is NOT done: writing the patch. Locating the site proves where the
			// code is; it does not prove that a branch written over it behaves. That needs the
			// game running, and shipping an untested code patch is how a mod corrupts a save. So
			// the address is reported and nothing is written.
			a_reason = capSite.note +
					   ", but the code patch itself is not written yet - that needs verifying in "
					   "game before it is safe to ship. Skills cap at 100 as in vanilla.";
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
		//
		// It MUST be reached through GetPlayerRuntimeData(). The member sits inside
		// PLAYER_RUNTIME_DATA, whose offset differs between runtimes (0x3D8 before 1.6.629,
		// 0x3E0 after), and CommonLibSSE-NG relocates it accordingly. Writing `player->skills`
		// compiles perfectly and reads the WRONG offset - measured 2026-09-03 in game: with a
		// save fully loaded and the player confirmed present, it came back null every time and
		// the tab reported "no character loaded". A compile is not evidence for a runtime layout.
		auto* skills = player->GetPlayerRuntimeData().skills;
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
