#include "PCH.h"

#include "Skills.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

// The two symbols the assembly stub in SkillCapStub.asm refers to. They are defined here, in
// ordinary C++, so the stub is nothing but the register shuffling that has to be assembly.
//
// CPC_GetSkillCap is a pure lookup: given the skill's ActorValue id it answers with the cap the
// player configured, or vanilla's 100 when this feature is off or the id is not a skill. It is
// deliberately silent - it can be reached often, and the standing rule is that nothing on a hot
// path logs unconditionally.
extern "C"
{
	std::uintptr_t CPC_SkillCapReturn = 0;   // where the stub resumes; set at install time
	void CPC_SkillCapStub();                 // defined in SkillCapStub.asm

	float CPC_GetSkillCap(std::uint32_t a_skillId)
	{
		constexpr float kVanillaCap = 100.0F;
		if (!settings::skills::overrideCaps) { return kVanillaCap; }
		if (a_skillId < 6 || a_skillId > 23) { return kVanillaCap; }
		return settings::skills::cap[a_skillId - 6];
	}
}

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

			// Step two, still NOT done: redirecting the game to the stub. The stub itself is
			// built and linked (SkillCapStub.asm, and CPC_GetSkillCap above), and the address it
			// would be attached to is verified - but attaching it rewrites an instruction inside
			// the running game, and that has never been executed once. It stays unattached until
			// it has been watched working, because the failure mode is a crash rather than a
			// wrong number.
			a_reason = capSite.note +
					   ". The cap stub is built and its target verified, but it is not attached "
					   "yet - that rewrites a live instruction and has to be watched working "
					   "first. Skills cap at 100 as in vanilla.";
			return false;
		}
	}

	void Register()
	{
		Patches::Register(
			"Skill caps",
			"Where a skill stops advancing, and the value the game's own formulas read for it.",
			InstallCapPatch);

		// The remaining groups have their settings surfaces built and saved, so a configuration
		// made now is ready the day each hook lands. They are registered rather than omitted
		// precisely so the Patches tab lists what is NOT active - a feature that is configurable
		// but inert has to say so, or the settings page is a lie.
		Patches::Register(
			"Skill experience rates",
			"What one use of a skill pays toward that skill, and what a skill increase pays "
			"toward a character level.",
			[](std::string& a_reason) {
				a_reason = "the settings are live and saved, but the hook that applies them is not "
						   "written yet, so experience is earned exactly as in vanilla";
				return false;
			});

		Patches::Register(
			"Level up rewards",
			"Perk points granted per level, and the health/magicka/stamina and carry weight a "
			"level up grants.",
			[](std::string& a_reason) {
				a_reason = "the settings are live and saved, but the hook that applies them is not "
						   "written yet, so a level up grants exactly what vanilla grants";
				return false;
			});

		Patches::Register(
			"Static levelling",
			"A fixed experience amount per use of a skill, instead of the vanilla scaling.",
			[](std::string& a_reason) {
				a_reason = "the settings are live and saved, but the hook that applies them is not "
						   "written yet, so skill experience still scales the vanilla way";
				return false;
			});
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
