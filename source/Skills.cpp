#include "PCH.h"

#include "Skills.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

#include <atomic>
#include <cstring>
#include <format>

// The two symbols the assembly stub in SkillCapStub.asm refers to. They are defined here, in
// ordinary C++, so the stub is nothing but the register shuffling that has to be assembly.
//
// CPC_GetSkillCap is a pure lookup: given the skill's ActorValue id it answers with the cap the
// player configured, or vanilla's 100 when this feature is off or the id is not a skill. It is
// deliberately silent - it can be reached often, and the standing rule is that nothing on a hot
// path logs unconditionally.
extern "C"
{
	void CPC_SkillCapStubXmm8();             // SkillCapStub.asm - SE 1.5.97 shape, cap in XMM8
	void CPC_SkillCapStubXmm10();            // SkillCapStub.asm - AE shape, cap in XMM10

	float CPC_GetSkillCap(std::uint32_t a_skillId)
	{
		constexpr float kVanillaCap = 100.0F;
		float r = kVanillaCap;
		if (settings::skills::overrideCaps && a_skillId >= 6 && a_skillId <= 23) { r = settings::skills::cap[a_skillId - 6]; }
		// Bounded trace (the first 8 calls only): which id the stub handed over and what went back. It
		// is how the patch was watched working; at trace it costs nothing on a normal run.
		static std::atomic<int> s_diag{ 0 };
		if (const int n = s_diag.fetch_add(1); n < 8) { logger::trace("cap-stub call #{}: skillId={} -> cap={:.1f}", n, a_skillId, r); }
		return r;
	}
}

namespace Skills
{
	namespace
	{
		// The site, on Skyrim SE 1.5.97, located through Address Library rather than a byte
		// signature. The working reference - Kasplat's Skyrim Skill Uncapper, whose Nexus permissions
		// allow reading its source - hooks the skill-advance function at Address Library ID 41561 +
		// 0x76: a 9-byte `movss xmm10, [rip+cap]` that loads the constant 100.0, followed by
		// `comiss xmm0, xmm10`. Contract there: XMM0 = the current level, XMM10 = the maximum.
		//
		// The 2017 byte signature this mod used before ALSO matched exactly once on 1.5.97 - but at a
		// DIFFERENT comparison in that function (a `movss xmm8` site), which is exactly why "found
		// once" is not enough: measured in game 2026-09-05, the stub received the right skill id,
		// returned the right cap, and broke the wrong check. So the bytes AND the constant they load
		// are verified before anything is written.
		// The 1.5.97 shape of the site, from the reference's own listing of the function: the virtual
		// call that fetches the level, its copy into xmm8, the cap load into xmm10, the compare and the
		// branch. (Kasplat's Address Library ID 41561 + 0x76 is that mod's AE location; on SE 1.5.97 it
		// resolves to unrelated bytes - measured 2026-09-05 - so the site is found by shape here.)
		// Two shapes of the same site. AE (Kasplat's listing): the level copied to xmm8, the cap loaded
		// into xmm10, `comiss xmm0, xmm10`. SE 1.5.97 (Kassent's 2017 uncapper, measured present here
		// 2026-09-05): the cap loaded straight into xmm8, `movaps xmm6, xmm0`, `comiss xmm6, xmm8`.
		// Each is matched exactly once and then proven by the register it loads and the 100.0 constant.
		constexpr const char* kSkillCapSigAE = "FF 50 18 44 0F 28 C0 F3 44 0F 10 15 ?? ?? ?? ?? 41 0F 2F C2 0F 83";     // load at +7, modrm 15 = xmm10
		constexpr const char* kSkillCapSigSE = "48 8B 01 FF 50 18 F3 44 0F 10 05 ?? ?? ?? ?? 0F 28 F0 41 0F 2F F0 0F 83"; // load at +6, modrm 05 = xmm8
		constexpr float kVanillaCapConstant = 100.0F;
		constexpr std::uint8_t kMovssRipPrefix[4] = { 0xF3, 0x44, 0x0F, 0x10 };   // movss xmm8-15, [rip+disp32]
		std::uintptr_t g_capSite = 0;

		bool InstallCapPatch(std::string& a_reason)
		{
			// Locate the load by shape - the longer anchor first, the shorter as a fallback - and it
			// must match exactly once. The byte and constant checks below then prove it is the cap load.
			std::uint8_t wantModrm = 0x15; const char* regName = "xmm10"; void (*stub)() = &CPC_SkillCapStubXmm10;
			Signature::Result found = Signature::Find(kSkillCapSigAE, 7);
			if (!found.found)
			{
				found = Signature::Find(kSkillCapSigSE, 6);
				wantModrm = 0x05; regName = "xmm8"; stub = &CPC_SkillCapStubXmm8;
			}
			if (!found.found)
			{
				a_reason = found.note + ". Skills cap at 100 as in vanilla.";
				return false;
			}
			const auto site = found.address;
			const auto base = REL::Module::get().base();
			const auto* b = reinterpret_cast<const std::uint8_t*>(site);
			if (std::memcmp(b, kMovssRipPrefix, sizeof(kMovssRipPrefix)) != 0 || b[4] != wantModrm)
			{
				a_reason = std::format("refused: game offset 0x{:X} does not hold the expected `movss {}, [rip+cap]` "
									   "({:02X} {:02X} {:02X} {:02X} {:02X}); nothing was written. Skills cap at 100 "
									   "as in vanilla.",
									   site - base, regName, b[0], b[1], b[2], b[3], b[4]);
				return false;
			}
			// The instruction is RIP-relative: resolve the constant it loads and insist it is 100.0.
			const auto disp = *reinterpret_cast<const std::int32_t*>(site + 5);
			const auto constAddr = site + 9 + static_cast<std::intptr_t>(disp);
			const float loaded = *reinterpret_cast<const float*>(constAddr);
			if (loaded != kVanillaCapConstant)
			{
				a_reason = std::format("refused: the constant that instruction loads is {} rather than 100.0, so "
									   "this is not the skill-cap load; nothing was written. Skills cap at 100 "
									   "as in vanilla.", loaded);
				return false;
			}
			g_capSite = site;
			const std::string located = std::format(
				"verified at game offset 0x{:X} (movss {} loading the 100.0 constant, matched once by shape)",
				site - base, regName);

			// Gated on the setting, so with "Control skill caps" off not a byte of the game is
			// written - the patch is opt-in, and turning it on takes a restart (this runs once).
			if (!settings::skills::overrideCaps)
			{
				a_reason = located + ". Ready but not attached: Control skill caps is off. Turn it on and "
									 "restart to attach it. Skills cap at 100 as in vanilla.";
				return false;
			}

			// Attach: a 5-byte call through the trampoline. The stub returns to site+5, where the
			// 4 NOPs carry execution on to the instruction after the original 9-byte load.
			auto& trampoline = SKSE::GetTrampoline();
			trampoline.write_call<5>(site, reinterpret_cast<std::uintptr_t>(stub));
			REL::safe_fill(site + 5, 0x90, 4);
			logger::info("Skill caps: attached at game offset 0x{:X} ({} shape; the 100.0 constant sits at 0x{:X}); "
						 "the cap now comes from this mod's settings",
						 site - base, regName, constAddr - base);
			a_reason = located + ". Attached: each skill now stops advancing at the cap set on the Skills "
								 "tab. (The formula cap is its own patch group, listed next.)";
			return true;
		}

		// ---- Formula caps: what the game's own formulas read for a skill --------------------------
		//
		// The reference (Skyrim Skill Uncapper) hooks the player's GetActorValue at its entry and
		// clamps skill reads to the formula cap. The same thing, done the CommonLibSSE-NG way: a
		// vtable hook on the player's ActorValueOwner (vfunc 01 = GetActorValue), which rewrites no
		// instruction at all. The vtable is not assumed - the live player object's own vtable pointer
		// is matched against PlayerCharacter's known vtables, and if none matches nothing is written.
		//
		// This runs on a hot path (every read of a player actor value), so it does one compare per
		// call and never logs. One known cosmetic consequence, inherited from the reference: the
		// vanilla skills menu reads through this too, so above the formula cap it shows the capped
		// value, not the true level. This mod's own Skills tab reads the base value and is unaffected.
		using GetActorValue_t = float(RE::ActorValueOwner*, RE::ActorValue);
		REL::Relocation<GetActorValue_t> g_origGetActorValue;

		float GetActorValue_Hook(RE::ActorValueOwner* a_this, RE::ActorValue a_av)
		{
			float v = g_origGetActorValue(a_this, a_av);
			const auto id = static_cast<std::uint32_t>(a_av);
			if (settings::skills::overrideCaps && id >= 6 && id <= 23)
			{
				const float cap = settings::skills::formulaCap[id - 6];
				if (v > cap) { v = cap; }
				if (v < 0.0F) { v = 0.0F; }
			}
			return v;
		}

		bool InstallFormulaCapPatch(std::string& a_reason)
		{
			if (!settings::skills::overrideCaps)
			{
				a_reason = "ready but not attached: Control skill caps is off. Turn it on and restart to "
						   "attach it. Formulas read the true skill value, as in vanilla.";
				return false;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* avo = player ? player->AsActorValueOwner() : nullptr;
			if (!avo)
			{
				a_reason = "the player object was not available when patches were installed; nothing was written";
				return false;
			}
			const auto liveVtbl = *reinterpret_cast<const std::uintptr_t*>(avo);
			int idx = -1;
			for (std::size_t i = 0; i < std::size(RE::VTABLE_PlayerCharacter); ++i)
			{
				if (RE::VTABLE_PlayerCharacter[i].address() == liveVtbl) { idx = static_cast<int>(i); break; }
			}
			if (idx < 0)
			{
				a_reason = std::format("refused: the player's ActorValueOwner vtable (0x{:X}) matched none of "
									   "PlayerCharacter's known vtables; nothing was written",
									   liveVtbl - REL::Module::get().base());
				return false;
			}
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[static_cast<std::size_t>(idx)] };
			g_origGetActorValue = vtbl.write_vfunc(1, GetActorValue_Hook);   // 01 = GetActorValue
			logger::info("Formula caps: hooked the player's ActorValueOwner::GetActorValue (PlayerCharacter vtable {}, "
						 "0x{:X})", idx, liveVtbl - REL::Module::get().base());
			a_reason = std::format("attached: the game's formulas read each skill at its formula cap (PlayerCharacter "
								   "vtable {}). The vanilla skills menu shows the capped value above the cap; this "
								   "mod's Skills tab shows the true level.", idx);
			return true;
		}
	}

	void Register()
	{
		Patches::Register(
			"Skill caps",
			"Where a skill stops advancing.",
			InstallCapPatch);

		Patches::Register(
			"Formula caps",
			"The value the game's own formulas read for a skill - so a skill can show 300 while the "
			"combat maths treats it as its formula cap.",
			InstallFormulaCapPatch);
	}

	std::uintptr_t CapSiteAddress() { return g_capSite; }

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
