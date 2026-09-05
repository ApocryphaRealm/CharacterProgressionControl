#include "PCH.h"

#include "SkillExperience.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"
#include "SkillPoints.h"
#include "Skills.h"

#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <format>

extern "C"
{
	// SkillCapStub.asm: takes the place of the game's `call` to its level-experience function -
	// calls the original with xmm0..xmm3 untouched, scales the result through CPC_LevelExp_Hook,
	// and returns to the game's call site.
	std::uintptr_t CPC_LevelExpOriginal = 0;   // the game's level-experience function; set at install
	void CPC_LevelExpCallStub();
}

namespace SkillExperience
{
	namespace
	{
		// The skill-improve function's prologue on Skyrim SE (from Kassent's 2017 SE uncapper;
		// present on 1.5.97 - measured 2026-09-05): mov r11,rsp / push rbp / push rsi / push r14 /
		// sub rsp,160h / lea eax,[rdx-6] / movaps [r11-38h],xmm7 / mov rbp,r9 / movaps xmm7,xmm2 /
		// mov esi,edx (the skill id every later site reads from rsi) / mov r14,rcx / cmp eax,11h.
		constexpr const char* kImproveSig =
			"4C 8B DC 55 56 41 56 48 81 EC 60 01 00 00 8D 42 FA 41 0F 29 7B C8 49 8B E9 0F 28 FA 8B F2 4C 8B F1 83 F8 11";
		// The three whole instructions a 5-byte branch at the entry overwrites - re-executed verbatim
		// by the "original" thunk (they are position-independent), then a jump to entry+5.
		constexpr std::uint8_t kPrologue5[5] = { 0x4C, 0x8B, 0xDC, 0x55, 0x56 };
		// The level-experience function - "what a skill increase pays toward a level". On 1.5.97 it is a
		// real leaf function (measured 2026-09-05 from a byte dump): `sub rsp,18h` and then this shape,
		// ending in `add rsp,18h; ret`. The shape starts 4 bytes AFTER the entry - an earlier version
		// looked for calls to the shape itself and found none - and the function reads xmm0..xmm3, so
		// the wrapper passes all four through. The skill-improve function calls it exactly once.
		constexpr const char* kLevelExpSig =
			"F3 0F 58 D3 0F 28 E0 0F 29 34 24 0F 57 F6 0F 28 CA F3 0F 58 CB F3 0F 59 CA F3 0F 59 CD F3 0F 2C C1";
		constexpr std::ptrdiff_t kImproveScan = 0x1800;  // the function is long; the call sits well past the entry
		constexpr std::ptrdiff_t kLevelExpEntryScan = 0x60;                    // the entry sits within this many bytes before the shape
		constexpr std::uint8_t kLevelExpPrologue[4] = { 0x48, 0x83, 0xEC, 0x18 }; // sub rsp,18h
		constexpr std::uint8_t kLevelExpEpilogue[5] = { 0x48, 0x83, 0xC4, 0x18, 0xC3 }; // add rsp,18h; ret

		using Improve_t = void(RE::PlayerCharacter::PlayerSkills*, std::uint32_t, float, std::uint64_t, std::uint32_t, std::uint8_t, bool);
		REL::Relocation<Improve_t> g_origImprove;
		std::uintptr_t g_improveEntry = 0;

		void Improve_Hook(RE::PlayerCharacter::PlayerSkills* a_skills, std::uint32_t a_skill, float a_exp,
						  std::uint64_t a_1, std::uint32_t a_2, std::uint8_t a_3, bool a_4)
		{
			const float incoming = a_exp;
			if (a_skill >= 6 && a_skill <= 23)
			{
				const auto i = a_skill - 6;
				if (SkillPoints::Applying())
				{
					// a point-bought increase: the amount is already exactly what lands the level
				}
				else if (settings::staticlevel::pointsEnabled)
				{
					a_exp = 0.0F;   // skills advance only by points spent at level up
				}
				else if (settings::staticlevel::enabled && a_skills && a_skills->data)
				{
					// Static: every use is the same SHARE of the current skill level, whatever the game
					// computed for it - so the level-5 and level-50 rates are equal, and perk modifiers
					// on skill use are ignored (the amount stays genuinely fixed).
					// The game multiplies what arrives here by the skill's own use multiplier and adds its use
					// offset (the AVIF's AVSK block; measured 2026-09-05: 1 in -> 6.3 banked for One-Handed), so the
					// share is expressed in those units to land exactly.
					float target = a_skills->data->skills[i].levelThreshold * settings::staticlevel::xpPerUse[i] / 100.0F;
#if RUNTIME_LINE == 17
					{
						if (auto* info = RE::ActorValueList::GetActorValueInfo(static_cast<RE::ActorValue>(a_skill)); info && info->skill && info->skill->useMult != 0.0F)
#else
					if (auto* list = RE::ActorValueList::GetSingleton())
					{
						if (auto* info = list->GetActorValue(static_cast<RE::ActorValue>(a_skill)); info && info->skill && info->skill->useMult != 0.0F)
#endif
						{
							target = (target - info->skill->offsetMult) / info->skill->useMult;
						}
					}
					a_exp = target < 0.0F ? 0.0F : target;
				}
				else if (settings::skillexp::overrideRates)
				{
					a_exp *= settings::skillexp::mult[i];
				}
			}
			{
				// TRACE (first 24 calls): how many times one use enters here, and with what.
				static std::atomic<int> n{ 0 };
				const int k = n.fetch_add(1);
				if (k < 24 && a_skill >= 6 && a_skill <= 23)
				{
					const bool ok = a_skills && a_skills->data;
					logger::trace("improve call #{}: skill {} in {:.2f} -> out {:.2f} (level {:.1f}, xp {:.1f}/{:.1f}) args {} {} {} {}", k, a_skill, incoming, a_exp,
								 ok ? a_skills->data->skills[a_skill - 6].level : -1.0F, ok ? a_skills->data->skills[a_skill - 6].xp : -1.0F,
								 ok ? a_skills->data->skills[a_skill - 6].levelThreshold : -1.0F, a_1, a_2, a_3, a_4);
				}
			}
			g_origImprove(a_skills, a_skill, a_exp, a_1, a_2, a_3, a_4);
		}

		// Locate the skill-improve function once, and prove it: the shape matches exactly once, the
		// entry carries the prologue bytes the thunk will re-execute, and the skill-cap site this mod
		// already verified lies inside it.
		bool LocateImprove(std::string& a_reason)
		{
			if (g_improveEntry) { return true; }
			const auto found = Signature::Find(kImproveSig, 0);
			if (!found.found) { a_reason = found.note; return false; }
			const auto entry = found.address;
			const auto base = REL::Module::get().base();
			if (std::memcmp(reinterpret_cast<const void*>(entry), kPrologue5, sizeof(kPrologue5)) != 0)
			{
				a_reason = std::format("refused: game offset 0x{:X} does not start with the expected prologue; nothing was written", entry - base);
				return false;
			}
			const auto capSite = Skills::CapSiteAddress();
			if (capSite && !(capSite > entry && capSite < entry + kImproveScan))
			{
				a_reason = std::format("refused: the verified skill-cap site (0x{:X}) does not lie inside the function found at 0x{:X}; nothing was written", capSite - base, entry - base);
				return false;
			}
			g_improveEntry = entry;
			return true;
		}

		// Attaches the entry hook once; both the rates group and the static group ride it.
		bool AttachImprove(std::string& a_reason)
		{
			if (g_origImprove.address()) { return true; }
			const auto base = REL::Module::get().base();
			// The "original": the 5 overwritten prologue bytes, then `jmp [rip+0]` to entry+5.
			auto& trampoline = SKSE::GetTrampoline();
			auto* thunk = static_cast<std::uint8_t*>(trampoline.allocate(5 + 6 + 8));
			if (!thunk) { a_reason = "refused: no trampoline space for the prologue thunk; nothing was written"; return false; }
			std::memcpy(thunk, kPrologue5, 5);
			thunk[5] = 0xFF; thunk[6] = 0x25; std::memset(thunk + 7, 0, 4);
			const std::uintptr_t resume = g_improveEntry + 5;
			std::memcpy(thunk + 11, &resume, 8);
			g_origImprove = REL::Relocation<Improve_t>(reinterpret_cast<std::uintptr_t>(thunk));
			trampoline.write_branch<5>(g_improveEntry, Improve_Hook);
			logger::info("skill-improve entry hook attached at game offset 0x{:X}", g_improveEntry - base);
			return true;
		}

		bool InstallRates(std::string& a_reason)
		{
			if (!LocateImprove(a_reason)) { a_reason += ". Experience is earned exactly as in vanilla."; return false; }
			const auto base = REL::Module::get().base();
			if (!settings::skillexp::overrideRates)
			{
				a_reason = std::format("verified at game offset 0x{:X} (the skill-improve function). Ready but not applied: "
									   "Control skill experience rates is off. Turn it on and restart to apply it.",
									   g_improveEntry - base);
				return false;
			}
			if (!AttachImprove(a_reason)) { return false; }
			logger::info("Skill experience rates: applied at the skill-improve entry, game offset 0x{:X}; each use now pays "
						 "fMult<Skill> times vanilla", g_improveEntry - base);
			a_reason = std::format("attached at game offset 0x{:X}: each use of a skill pays vanilla times that skill's "
								   "Experience rate.", g_improveEntry - base);
			return true;
		}

		bool InstallStatic(std::string& a_reason)
		{
			if (!LocateImprove(a_reason)) { a_reason += ". Skill experience scales exactly as in vanilla."; return false; }
			const auto base = REL::Module::get().base();
			if (!settings::staticlevel::enabled)
			{
				// Skill points ride the same hook (ordinary experience must not bank while they are on).
				if (settings::staticlevel::pointsEnabled)
				{
					if (!AttachImprove(a_reason)) { return false; }
					logger::info("Static levelling: the skill-improve hook is attached for skill points (ordinary skill experience is not banked)");
				}
				a_reason = std::format("verified at game offset 0x{:X} (the skill-improve function). Ready but not applied: "
									   "Use static skill levelling is off. Turn it on and restart to apply it.{}",
									   g_improveEntry - base, settings::staticlevel::pointsEnabled ? " (Skill points are on, so ordinary skill experience is not banked.)" : "");
				return false;
			}
			if (!AttachImprove(a_reason)) { return false; }
			logger::info("Static levelling: applied at the skill-improve entry, game offset 0x{:X}; each use now pays "
						 "fPerUse<Skill> percent of a skill level, whatever the game computed", g_improveEntry - base);
			a_reason = std::format("attached at game offset 0x{:X}: each use of a skill pays that skill's Experience per use "
								   "as a share of a level, at every level; the vanilla scaling and perk modifiers on skill use "
								   "are not applied.", g_improveEntry - base);
			return true;
		}

		bool InstallLevelIncome(std::string& a_reason)
		{
			if (!LocateImprove(a_reason)) { a_reason += ". A skill increase pays toward a level exactly as in vanilla."; return false; }
			const auto base = REL::Module::get().base();
			const auto maths = Signature::Find(kLevelExpSig, 0);
			if (!maths.found) { a_reason = maths.note + ". A skill increase pays toward a level exactly as in vanilla."; return false; }
			// Prove it is a self-contained function: its prologue `sub rsp,18h` is the first instruction after the
			// previous function's padding or `ret` (measured 2026-09-05: constant loads sit between the prologue
			// and the shape, so the entry is not a fixed distance back), and its epilogue is within the shape.
			std::uintptr_t fnEntry = 0;
			for (std::uintptr_t p = maths.address - 4; p >= maths.address - kLevelExpEntryScan; --p)
			{
				if (std::memcmp(reinterpret_cast<const void*>(p), kLevelExpPrologue, 4) != 0) { continue; }
				const auto before = *reinterpret_cast<const std::uint8_t*>(p - 1);
				if (before == 0xCC || before == 0xC3) { fnEntry = p; break; }
			}
			if (!fnEntry)
			{
				std::string hex;
				for (std::uintptr_t p = maths.address - kLevelExpEntryScan; p < maths.address; ++p) { hex += std::format("{:02X} ", *reinterpret_cast<const std::uint8_t*>(p)); }
				logger::info("bytes before the level-experience shape (shape-0x{:X}..shape): {}", kLevelExpEntryScan, hex);
				a_reason = std::format("refused: no function-boundary `sub rsp,18h` within 0x{:X} bytes before the level-experience shape at 0x{:X}; "
									   "nothing was written", kLevelExpEntryScan, maths.address - base);
				return false;
			}
			bool epilogue = false;
			for (std::uintptr_t p = maths.address; p + 5 <= maths.address + 0x100; ++p)
			{
				if (std::memcmp(reinterpret_cast<const void*>(p), kLevelExpEpilogue, 5) == 0) { epilogue = true; break; }
			}
			if (!epilogue)
			{
				a_reason = std::format("refused: no `add rsp,18h; ret` after the level-experience shape at 0x{:X}; nothing was written", maths.address - base);
				return false;
			}
			// The one call to it inside the skill-improve function is the site.
			std::uintptr_t callSite = 0; int calls = 0;
			const auto scanEnd = std::min<std::uintptr_t>(g_improveEntry + kImproveScan, fnEntry);
			for (std::uintptr_t p = g_improveEntry; p + 5 <= scanEnd; ++p)
			{
				if (*reinterpret_cast<const std::uint8_t*>(p) != 0xE8) { continue; }
				const auto rel = *reinterpret_cast<const std::int32_t*>(p + 1);
				if (p + 5 + static_cast<std::intptr_t>(rel) == fnEntry) { callSite = p; ++calls; }
			}
			if (calls != 1)
			{
				a_reason = std::format("refused: expected exactly one call to the level-experience function (0x{:X}) inside the "
									   "skill-improve function, found {}; nothing was written", fnEntry - base, calls);
				return false;
			}
			if (!settings::skillexp::overrideRates)
			{
				a_reason = std::format("verified: the level-experience call at game offset 0x{:X} (function at 0x{:X}). Ready but not "
									   "attached: Control skill experience rates is off. Turn it on and restart to attach it.",
									   callSite - base, fnEntry - base);
				return false;
			}
			CPC_LevelExpOriginal = fnEntry;
			SKSE::GetTrampoline().write_call<5>(callSite, reinterpret_cast<std::uintptr_t>(&CPC_LevelExpCallStub));
			logger::info("Skill-to-level income: the level-experience call at game offset 0x{:X} now goes through this mod "
						 "(function at 0x{:X}); a skill increase pays fSkillToLevelMult times vanilla", callSite - base, fnEntry - base);
			a_reason = std::format("attached at game offset 0x{:X}: a skill increase pays vanilla times the Skill increase -> "
								   "level multiplier.", callSite - base);
			return true;
		}
	}

	void Register()
	{
		Patches::Register(
			"Skill experience rates",
			"What one use of a skill pays toward that skill.",
			InstallRates);
		Patches::Register(
			"Skill-to-level income",
			"What a skill increase pays toward a character level.",
			InstallLevelIncome);
		Patches::Register(
			"Static levelling",
			"A fixed share of a skill level per use of that skill, instead of the vanilla scaling.",
			InstallStatic);
	}
}

extern "C" float CPC_LevelExp_Hook(float a_value, std::uint32_t a_skillId)
{
	(void)a_skillId;
	if (SkillPoints::Applying()) { return 0.0F; }   // a point-spent level pays nothing toward the character level
	return settings::skillexp::overrideRates ? a_value * settings::skillexp::toLevelMult : a_value;
}
