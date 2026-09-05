#include "PCH.h"

#include "Attributes.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

#include <atomic>
#include <cstring>
#include <format>

namespace Attributes
{
	namespace
	{
		// Kassent's SE pattern sits 0x1E bytes into the routine; the entry is `push rdi; sub rsp,30h`.
		constexpr const char* kSig =
			"0F B6 DA 48 8B F9 48 8B 15 ?? ?? ?? ?? 48 81 C2 28 01 00 00 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? "
			"84 C0 0F 84 BA 00 00 00 84 DB 0F 85 AA 00 00 00";
		constexpr std::ptrdiff_t kEntryBack = 0x1E;
		constexpr std::uint8_t kPrologue6[6] = { 0x40, 0x57, 0x48, 0x83, 0xEC, 0x30 };   // push rdi; sub rsp,30h
		// The chosen attribute, as the reference reads it: an ActorValue id at +0x18 of the first argument.
		constexpr std::ptrdiff_t kChoiceOffset = 0x18;
		constexpr std::uint32_t kHealth = 0x18, kMagicka = 0x19, kStamina = 0x1A;
		// Measured 2026-09-05 at entry+0x96: `cmp dword [rdi+18h],1Ah; jne` - the game applies the carry-weight
		// gain only when stamina was chosen. Two NOPs over the jne make it apply on every choice, with the
		// amount per choice coming from the hook above (0 where none is wanted).
		constexpr std::ptrdiff_t kGateOffset = 0x96;
		constexpr std::uint8_t kGate[6] = { 0x83, 0x7F, 0x18, 0x1A, 0x75, 0x22 };

		using Fn_t = std::uint64_t(void*, std::uint8_t);
		REL::Relocation<Fn_t> g_orig;
		std::uintptr_t g_entry = 0;
		std::uintptr_t g_choose = 0;   // the LevelUp Menu's choose-attribute function (menu, av) - test driving only

		RE::Setting* Find(const char* a_name)
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			return gs ? gs->GetSetting(a_name) : nullptr;
		}

		std::uint64_t Hook(void* a_unk0, std::uint8_t a_unk1)
		{
			logger::trace("attribute level-up: hook entered (arg0 {}, arg1 {})", a_unk0 != nullptr, a_unk1);
			if (!settings::levelup::overrideRewards || !a_unk0) { return g_orig(a_unk0, a_unk1); }
			const auto choice = *reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(a_unk0) + kChoiceOffset);
			auto* gain = Find("iAVDhmsLevelUp");
			auto* carry = Find("fLevelUpCarryWeightMod");
			if (!gain || !carry || (choice != kHealth && choice != kMagicka && choice != kStamina))
			{
				static std::atomic<bool> once{ false };
				if (!once.exchange(true))
				{
					logger::warn("attribute level-up: could not apply this mod's values (choice {}, settings {}/{}) - the game's own applied",
								 choice, gain != nullptr, carry != nullptr);
				}
				return g_orig(a_unk0, a_unk1);
			}
			const auto vanillaGain = gain->data.i;
			const auto vanillaCarry = carry->data.f;
			float amount = 10.0F, cw = 0.0F;
			switch (choice)
			{
			case kHealth:  amount = settings::levelup::healthPerLevel;  cw = settings::levelup::carryWeightPerHealth;  break;
			case kMagicka: amount = settings::levelup::magickaPerLevel; cw = settings::levelup::carryWeightPerMagicka; break;
			default:       amount = settings::levelup::staminaPerLevel; cw = settings::levelup::carryWeightPerStamina; break;
			}
			gain->data.i = static_cast<std::int32_t>(amount + 0.5F);
			carry->data.f = cw;
			logger::info("attribute level-up: choice {} -> +{} attribute, +{:.1f} carry weight (vanilla {} / {:.1f})",
						 choice == kHealth ? "health" : choice == kMagicka ? "magicka" : "stamina", gain->data.i, cw, vanillaGain, vanillaCarry);
			const auto r = g_orig(a_unk0, a_unk1);
			gain->data.i = vanillaGain;
			carry->data.f = vanillaCarry;
			return r;
		}

		// The LevelUp Menu's choose-attribute function - what its addHealth/addMagicka/addStamina delegate
		// handlers jump to - resolved from the "addHealth" string: its one code reference is the delegate
		// registration, the handler's address is loaded right beside it, and the handler is three
		// instructions ending in a jump to the function. Testing only: the cpc.control levelup op drives
		// the game's own level-up with it, headlessly. Not a patch site; nothing is written.
		void ResolveChooseFunction()
		{
			const auto& mod = REL::Module::get();
			const auto base = mod.base();
			const auto text = mod.segment(REL::Segment::textx);
			const auto rdata = mod.segment(REL::Segment::rdata);
			const std::uintptr_t t0 = text.address(), t1 = t0 + text.size();
			const char* needle = "addHealth";
			const std::size_t nlen = std::strlen(needle) + 1;
			std::uintptr_t strAddr = 0;
			for (std::uintptr_t p = rdata.address(); p + nlen <= rdata.address() + rdata.size(); ++p)
			{
				if (std::memcmp(reinterpret_cast<const void*>(p), needle, nlen) == 0) { strAddr = p; break; }
			}
			if (!strAddr) { logger::debug("levelup test op: \"addHealth\" string not found"); return; }
			for (std::uintptr_t p = t0; p + 4 <= t1 && !g_choose; ++p)
			{
				const auto rel = *reinterpret_cast<const std::int32_t*>(p);
				if (p + 4 + static_cast<std::intptr_t>(rel) != strAddr) { continue; }
				for (std::uintptr_t q = p - 0x20; q < p + 0x20 && !g_choose; ++q)
				{
					const auto* b2 = reinterpret_cast<const std::uint8_t*>(q);
					if ((b2[0] != 0x48 && b2[0] != 0x4C) || b2[1] != 0x8D || (b2[2] & 0xC7) != 0x05) { continue; }
					const auto r2 = *reinterpret_cast<const std::int32_t*>(q + 3);
					const auto tgt = q + 7 + static_cast<std::intptr_t>(r2);
					if (tgt < t0 || tgt >= t1) { continue; }
					const auto* h = reinterpret_cast<const std::uint8_t*>(tgt);
					// handler shape: mov rcx,[rcx+18h]; test rcx,rcx; jz +A; mov edx,<av>; jmp <choose>
					if (h[0] == 0x48 && h[1] == 0x8B && h[2] == 0x49 && h[3] == 0x18 && h[9] == 0xBA && h[14] == 0xE9)
					{
						const auto jrel = *reinterpret_cast<const std::int32_t*>(h + 15);
						g_choose = tgt + 19 + static_cast<std::intptr_t>(jrel);
						logger::debug("levelup test op: the LevelUp Menu's choose-attribute function is at game offset 0x{:X}", g_choose - base);
					}
				}
			}
			if (!g_choose) { logger::debug("levelup test op: choose-attribute function not resolved; the op is unavailable"); }
		}

		bool Install(std::string& a_reason)
		{
			const auto found = Signature::Find(kSig, -kEntryBack);
			if (!found.found) { a_reason = found.note + ". A level up grants exactly what vanilla grants."; return false; }
			const auto entry = found.address;
			const auto base = REL::Module::get().base();
			if (std::memcmp(reinterpret_cast<const void*>(entry), kPrologue6, 6) != 0)
			{
				const auto* b = reinterpret_cast<const std::uint8_t*>(entry);
				a_reason = std::format("refused: game offset 0x{:X} does not start with `push rdi; sub rsp,30h` ({:02X} {:02X} {:02X} {:02X} {:02X} {:02X}); nothing was written",
									   entry - base, b[0], b[1], b[2], b[3], b[4], b[5]);
				return false;
			}
			ResolveChooseFunction();
			g_entry = entry;
			if (!settings::levelup::overrideRewards)
			{
				a_reason = std::format("verified at game offset 0x{:X} (the attribute level-up routine). Ready but not attached: Control what a level up "
									   "grants is off. Turn it on and restart to attach it. A level up grants exactly what vanilla grants.", entry - base);
				return false;
			}
			// The "original": the 6 overwritten prologue bytes, then `jmp [rip+0]` to entry+6.
			auto& trampoline = SKSE::GetTrampoline();
			auto* thunk = static_cast<std::uint8_t*>(trampoline.allocate(6 + 6 + 8));
			if (!thunk) { a_reason = "refused: no trampoline space for the prologue thunk; nothing was written"; return false; }
			std::memcpy(thunk, kPrologue6, 6);
			thunk[6] = 0xFF; thunk[7] = 0x25; std::memset(thunk + 8, 0, 4);
			const std::uintptr_t resume = entry + 6;
			std::memcpy(thunk + 12, &resume, 8);
			g_orig = REL::Relocation<Fn_t>(reinterpret_cast<std::uintptr_t>(thunk));
			trampoline.write_branch<5>(entry, Hook);
			const bool gate = std::memcmp(reinterpret_cast<const void*>(entry + kGateOffset), kGate, 6) == 0;
			if (gate) { REL::safe_fill(entry + kGateOffset + 4, 0x90, 2); }
			else
			{
				const auto* b = reinterpret_cast<const std::uint8_t*>(entry + kGateOffset);
				logger::warn("attribute level-up: the stamina-only carry-weight gate was not at entry+0x{:X} ({:02X} {:02X} {:02X} {:02X} {:02X} {:02X}); "
							 "carry weight follows the stamina choice only", kGateOffset, b[0], b[1], b[2], b[3], b[4], b[5]);
			}
			logger::info("Attribute gains: attached at the attribute level-up routine, game offset 0x{:X}; carry weight on every choice: {}", entry - base, gate);
			a_reason = gate ? std::format("attached at game offset 0x{:X}: the chosen attribute and the carry weight for that choice come from this mod.", entry - base)
							: std::format("attached at game offset 0x{:X}: the chosen attribute comes from this mod; carry weight applies on the stamina "
										  "choice only (the game's gate was not where expected, so it was left alone).", entry - base);
			return true;
		}
	}

	std::uintptr_t ChooseAddress() { return g_choose; }

	void Register()
	{
		Patches::Register(
			"Attribute gains at level up",
			"The health/magicka/stamina and carry weight a level up grants.",
			Install);
	}
}
