#include "PCH.h"

#include "Attributes.h"

#include "CarryWeight.h"
#include "Compat.h"
#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <format>
#include <mutex>

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
		// amount per choice coming from the hook above (0 where none is wanted - and, with the level-up
		// control off, the game's own amount on stamina and 0 elsewhere, which is vanilla exactly).
		constexpr std::ptrdiff_t kGateOffset = 0x96;
		constexpr std::uint8_t kGate[6] = { 0x83, 0x7F, 0x18, 0x1A, 0x75, 0x22 };

		constexpr std::uint32_t kRecordVersion = 1;
		constexpr RE::ActorValue kAv[3] = { RE::ActorValue::kHealth, RE::ActorValue::kMagicka, RE::ActorValue::kStamina };
		constexpr const char* kName[3] = { "health", "magicka", "stamina" };

		using Fn_t = std::uint64_t(void*, std::uint8_t);
		REL::Relocation<Fn_t> g_orig;
		std::uintptr_t g_entry = 0;
		std::uintptr_t g_choose = 0;   // the LevelUp Menu's choose-attribute function (menu, av) - test driving only
		bool g_gateOpen = false;       // the stamina-only gate was NOPed, so the hook decides carry weight per choice
		std::atomic<bool> g_attached{ false };

		// The count and the starting-value modifiers, mirrored in the co-save. Written on the main
		// thread only (the hook runs inside the LevelUp Menu; Apply() is queued there).
		std::atomic<std::uint32_t> g_invested[3]{};
		float g_applied[3]{};
		std::uint16_t g_sinceLevel = 0;
		bool g_haveHistory = false;

		std::mutex g_stateLock;
		State g_state;

		RE::Setting* Find(const char* a_name)
		{
			auto* gs = RE::GameSettingCollection::GetSingleton();
			return gs ? gs->GetSetting(a_name) : nullptr;
		}

		int IndexOf(std::uint32_t a_choice)
		{
			return a_choice == kHealth ? 0 : a_choice == kMagicka ? 1 : a_choice == kStamina ? 2 : -1;
		}

		std::uint64_t Hook(void* a_unk0, std::uint8_t a_unk1)
		{
			logger::trace("attribute level-up: hook entered (arg0 {}, arg1 {})", a_unk0 != nullptr, a_unk1);
			if (!a_unk0) { return g_orig(a_unk0, a_unk1); }
			const auto choice = *reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(a_unk0) + kChoiceOffset);
			const int idx = IndexOf(choice);
			if (idx >= 0)
			{
				const auto n = ++g_invested[idx];
				logger::info("attribute level-up: {} chosen - {} time(s) counted since level {}", kName[idx], n, g_sinceLevel);
			}
			auto* gain = Find("iAVDhmsLevelUp");
			auto* carry = Find("fLevelUpCarryWeightMod");
			if (!gain || !carry || idx < 0)
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
			float amount = static_cast<float>(vanillaGain), cw = 0.0F;
			if (settings::levelup::overrideRewards)
			{
				switch (choice)
				{
				case kHealth:  amount = settings::levelup::healthPerLevel;  cw = settings::levelup::carryWeightPerHealth;  break;
				case kMagicka: amount = settings::levelup::magickaPerLevel; cw = settings::levelup::carryWeightPerMagicka; break;
				default:       amount = settings::levelup::staminaPerLevel; cw = settings::levelup::carryWeightPerStamina; break;
				}
			}
			else
			{
				// Not controlling: hand the game its own numbers. With the gate open the amount has to be
				// chosen here - vanilla is the carry-weight mod on stamina and nothing on the other two.
				cw = (choice == kStamina || !g_gateOpen) ? vanillaCarry : 0.0F;
			}
			// One owner for carry weight: the Carry Weight tab's formula, or one of our standalone mods.
			if (Compat::CarryWeightOwnedElsewhere() || CarryWeight::Controlling()) { cw = 0.0F; }
			gain->data.i = static_cast<std::int32_t>(amount + 0.5F);
			carry->data.f = cw;
			logger::info("attribute level-up: choice {} -> +{} attribute, +{:.1f} carry weight (vanilla {} / {:.1f}; control {})",
						 kName[idx], gain->data.i, cw, vanillaGain, vanillaCarry, settings::levelup::overrideRewards ? "on" : "off");
			const auto r = g_orig(a_unk0, a_unk1);
			gain->data.i = vanillaGain;
			carry->data.f = vanillaCarry;
			// The choice may have moved the permanent carry weight; the formula re-targets it, and the
			// Attributes readout refreshes.
			CarryWeight::RequestApply();
			RequestApply();
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
			if (!found.found) { a_reason = found.note + ". A level up grants exactly what vanilla grants, and attribute choices are not counted."; return false; }
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
			g_attached = true;
			const bool gate = std::memcmp(reinterpret_cast<const void*>(entry + kGateOffset), kGate, 6) == 0;
			if (gate) { REL::safe_fill(entry + kGateOffset + 4, 0x90, 2); g_gateOpen = true; }
			else
			{
				const auto* b = reinterpret_cast<const std::uint8_t*>(entry + kGateOffset);
				logger::warn("attribute level-up: the stamina-only carry-weight gate was not at entry+0x{:X} ({:02X} {:02X} {:02X} {:02X} {:02X} {:02X}); "
							 "carry weight follows the stamina choice only", kGateOffset, b[0], b[1], b[2], b[3], b[4], b[5]);
			}
			logger::info("Attribute gains: attached at the attribute level-up routine, game offset 0x{:X}; carry weight on every choice: {}; counting choices", entry - base, gate);
			a_reason = std::format("attached at game offset 0x{:X}: every attribute choice is counted for the Attributes tab, and while \"Control what a level up grants\" is on "
								   "the chosen attribute's gain and the carry weight for that choice come from this mod{}.",
								   entry - base, gate ? "" : " (carry weight on the stamina choice only - the game's gate was not where expected, so it was left alone)");
			return true;
		}

		void Modify(RE::ActorValueOwner* a_owner, RE::ActorValue a_av, float a_delta)
		{
#if RUNTIME_LINE == 17
			a_owner->ModBaseActorValue(a_av, a_delta);
#else
			a_owner->ModActorValue(a_av, a_delta);
#endif
		}
	}

	std::uintptr_t ChooseAddress() { return g_choose; }

	bool Counting() { return g_attached.load(); }

	void Register()
	{
		Patches::Register(
			"Attribute gains at level up",
			"The health/magicka/stamina and carry weight a level up grants, and the count of attribute choices behind the Attributes tab.",
			Install);
	}

	void Apply()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded()) { logger::debug("attributes: no loaded player yet"); return; }
		auto* owner = player->AsActorValueOwner();
		if (!owner) { logger::warn("attributes: the player has no actor-value owner; nothing applied"); return; }
		const auto* race = player->GetRace();

		State s;
		s.playerLevel = player->GetLevel();
		s.sinceLevel = g_sinceLevel;
		s.haveHistory = g_haveHistory;
		s.controlling = settings::attributes::control;
		const float perLevel[3] = { settings::levelup::healthPerLevel, settings::levelup::magickaPerLevel, settings::levelup::staminaPerLevel };
		for (int i = 0; i < 3; ++i)
		{
			auto& r = s.row[i];
			r.raceStart = race ? (i == 0 ? race->data.startingHealth : i == 1 ? race->data.startingMagicka : race->data.startingStamina) : 100.0F;
			r.perLevel = perLevel[i];
			r.invested = g_invested[i].load();
			r.permanent = owner->GetPermanentActorValue(kAv[i]);
			r.current = owner->GetActorValue(kAv[i]);
			// On: (starting - 100) on top of the race's own start. Off: nothing - this mod's share comes off.
			const float wanted = s.controlling ? (settings::attributes::starting[i] - 100.0F) : 0.0F;
			const float delta = wanted - g_applied[i];
			if (std::fabs(delta) > 0.01F)
			{
				Modify(owner, kAv[i], delta);
				g_applied[i] = wanted;
				logger::info("attributes: {} permanent {:.1f} -> {:.1f} (race start {:.1f}, this mod {:+.1f}; {})",
							 kName[i], r.permanent, r.permanent + delta, r.raceStart, wanted, s.controlling ? "starting values on" : "control off - this mod's share taken away");
				r.permanent = owner->GetPermanentActorValue(kAv[i]);
				r.current = owner->GetActorValue(kAv[i]);
			}
			r.applied = g_applied[i];
		}
		std::scoped_lock l(g_stateLock);
		g_state = s;
	}

	void RequestApply()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(Apply); }
	}

	void OnGameLoaded()
	{
		if (!g_haveHistory)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			g_sinceLevel = player ? player->GetLevel() : 1;
			g_haveHistory = true;
			for (auto& n : g_invested) { n = 0; }
			for (auto& a : g_applied) { a = 0.0F; }
			logger::info("attributes: no count yet for this character - counting attribute choices from level {} on{}", g_sinceLevel,
						 g_sinceLevel > 1 ? " (earlier level-ups are unknown and not inferred)" : "");
		}
		RequestApply();
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc || !a_intfc->OpenRecord(kRecord, kRecordVersion)) { logger::error("attributes: could not open the co-save record; the count was not written"); return; }
		const std::uint8_t have = g_haveHistory ? 1 : 0;
		a_intfc->WriteRecordData(&g_sinceLevel, sizeof(g_sinceLevel));
		a_intfc->WriteRecordData(&have, sizeof(have));
		for (int i = 0; i < 3; ++i) { const std::uint32_t n = g_invested[i].load(); a_intfc->WriteRecordData(&n, sizeof(n)); }
		a_intfc->WriteRecordData(g_applied, sizeof(g_applied));
	}

	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length)
	{
		OnRevert();
		constexpr std::uint32_t kLength = sizeof(std::uint16_t) + sizeof(std::uint8_t) + 3 * sizeof(std::uint32_t) + 3 * sizeof(float);
		if (!a_intfc || a_version != kRecordVersion || a_length != kLength)
		{
			logger::warn("attributes: co-save record version {} length {} not understood; the count starts again", a_version, a_length);
			return;
		}
		std::uint16_t since = 0; std::uint8_t have = 0; std::uint32_t n[3]{}; float applied[3]{};
		if (!a_intfc->ReadRecordData(&since, sizeof(since)) || !a_intfc->ReadRecordData(&have, sizeof(have)) ||
			!a_intfc->ReadRecordData(n, sizeof(n)) || !a_intfc->ReadRecordData(applied, sizeof(applied)))
		{
			logger::warn("attributes: short co-save record; the count starts again");
			OnRevert();
			return;
		}
		g_sinceLevel = since;
		g_haveHistory = have != 0;
		for (int i = 0; i < 3; ++i) { g_invested[i] = n[i]; g_applied[i] = applied[i]; }
		logger::debug("attributes: co-save loaded - since level {}, invested {}/{}/{}, applied {:.1f}/{:.1f}/{:.1f}",
					  g_sinceLevel, n[0], n[1], n[2], applied[0], applied[1], applied[2]);
	}

	void OnRevert()
	{
		g_sinceLevel = 0;
		g_haveHistory = false;
		for (auto& n : g_invested) { n = 0; }
		for (auto& a : g_applied) { a = 0.0F; }
		std::scoped_lock l(g_stateLock);
		g_state = State{};
	}

	State GetState()
	{
		std::scoped_lock l(g_stateLock);
		return g_state;
	}

	std::string StatusJson()
	{
		const auto s = GetState();
		std::string rows;
		for (int i = 0; i < 3; ++i)
		{
			const auto& r = s.row[i];
			rows += std::format(R"({}"{}":{{"raceStart":{:.1f},"starting":{:.1f},"applied":{:.1f},"perLevel":{:.1f},"invested":{},"permanent":{:.1f},"current":{:.1f}}})",
								i ? "," : "", kName[i], r.raceStart, settings::attributes::starting[i], r.applied, r.perLevel, r.invested, r.permanent, r.current);
		}
		return std::format(R"({{"control":{},"counting":{},"haveHistory":{},"sinceLevel":{},"playerLevel":{},{}}})",
						   settings::attributes::control ? "true" : "false", Counting() ? "true" : "false", s.haveHistory ? "true" : "false",
						   s.sinceLevel, s.playerLevel, rows);
	}
}
