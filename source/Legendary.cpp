#include "PCH.h"

#include "Legendary.h"

#include "Patches.h"
#include "Settings.h"
#include "Signature.h"

#include "utils/Logger.h"

#include <atomic>
#include <cfloat>
#include <cstring>
#include <format>

#include <windows.h>

namespace Legendary
{
	namespace
	{
		constexpr float kVanilla = 100.0F;

		// LegendarySkillResetConfirmCallback::skill. CommonLibSSE-NG lays the member out at 0x1C, and the
		// reference (Kassent's 2017 uncapper) reads the skill being reset from [rsi+1Ch] inside Run - the
		// same member, reached from the same object. Read raw so the two build lines need no member name.
		constexpr std::uintptr_t kSkillOffset = 0x1C;

		// ---- Legendary reset level ---------------------------------------------------------------

		RE::Setting* ResetSetting()
		{
			auto* collection = RE::GameSettingCollection::GetSingleton();
			return collection ? collection->GetSetting("fLegendarySkillResetValue") : nullptr;
		}

		float BaseLevel(std::uint32_t a_av)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* owner = player ? player->AsActorValueOwner() : nullptr;
			return owner ? owner->GetBaseActorValue(static_cast<RE::ActorValue>(a_av)) : -1.0F;
		}

		// Keep = the level the skill has now. Otherwise the chosen level (0 = the game's own value), and
		// never above the level the skill has now - making a skill legendary must not raise it.
		float ResetLevel(float a_gameReset, float a_base)
		{
			if (a_base < 0.0F) { return a_gameReset; }   // no reading of the skill: leave the game's own value
			if (settings::legendary::keepLevel) { return a_base; }
			const float chosen = settings::legendary::levelAfter > 0.0F ? settings::legendary::levelAfter : a_gameReset;
			return chosen < a_base ? chosen : a_base;
		}

		using Run_t = void(void*, std::int32_t);
		REL::Relocation<Run_t> g_origRun;
		std::atomic<bool> g_resetHooked{ false };
		std::atomic<int> g_runs{ 0 };
		std::atomic<std::uint32_t> g_lastSkill{ 0 };
		std::atomic<float> g_lastBefore{ -1.0F };
		std::atomic<float> g_lastWritten{ -1.0F };
		std::atomic<float> g_lastAfter{ -1.0F };

		// Runs once per answer to the game's own "make this skill legendary?" box - never per frame.
		void Run_Hook(void* a_this, std::int32_t a_msg)
		{
			RE::Setting* setting = settings::legendary::control ? ResetSetting() : nullptr;
			const std::uint32_t av = a_this ? *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<std::uintptr_t>(a_this) + kSkillOffset) : 0;
			if (!setting || av < 6 || av > 23)
			{
				logger::debug("legendary: Run(message {}) for actor value {} passed through untouched (control {}, setting {})",
							  a_msg, av, settings::legendary::control, setting ? "found" : "absent");
				g_origRun(a_this, a_msg);
				return;
			}
			const float gameReset = setting->data.f;
			const float before = BaseLevel(av);
			const float target = ResetLevel(gameReset, before);
			setting->data.f = target;
			g_origRun(a_this, a_msg);
			setting->data.f = gameReset;
			const float after = BaseLevel(av);
			g_runs.fetch_add(1);
			g_lastSkill = av;
			g_lastBefore = before;
			g_lastWritten = target;
			g_lastAfter = after;
			logger::info("legendary: Run(message {}) for skill {} - base level {:.1f} -> {:.1f}; {:.1f} was in fLegendarySkillResetValue "
						 "for the call (the game's own {:.1f}, put back)", a_msg, av, before, after, target, gameReset);
		}

		bool InstallReset(std::string& a_reason)
		{
			auto* setting = ResetSetting();
			if (!setting)
			{
				a_reason = "refused: the game setting fLegendarySkillResetValue does not exist on this runtime; nothing was written. "
						   "A legendary skill drops to the game's own level, as in vanilla.";
				return false;
			}
			const float gameReset = setting->data.f;
			const auto base = REL::Module::get().base();
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE___LegendarySkillResetConfirmCallback[0] };
			// Ask the object before writing to it (rule 30): the vtable's complete-object locator must point
			// at this class's own type descriptor.
			const auto col = *reinterpret_cast<const std::uintptr_t*>(vtbl.address() - sizeof(std::uintptr_t));
			const std::uint32_t tdRva = col ? *reinterpret_cast<const std::uint32_t*>(col + 0x0C) : 0;
			const auto td = RE::RTTI___LegendarySkillResetConfirmCallback.address();
			if (!col || base + tdRva != td)
			{
				a_reason = std::format("refused: the vtable at game offset 0x{:X} does not belong to LegendarySkillResetConfirmCallback "
									   "(its type descriptor is at 0x{:X}, expected 0x{:X}); nothing was written. A legendary skill "
									   "drops to {:.0f}, as in vanilla.", vtbl.address() - base, tdRva, td - base, gameReset);
				return false;
			}
			const std::string located = std::format("verified: LegendarySkillResetConfirmCallback's vtable at game offset 0x{:X}; the game's "
													"own reset level is {:.0f}", vtbl.address() - base, gameReset);
			if (!settings::legendary::control)
			{
				a_reason = located + ". Ready but not attached: Control legendary skills is off. Turn it on and restart to attach it.";
				return false;
			}
			g_origRun = vtbl.write_vfunc(1, Run_Hook);   // 01 = Run
			g_resetHooked = true;
			logger::info("Legendary reset level: hooked LegendarySkillResetConfirmCallback::Run (vtable at game offset 0x{:X})",
						 vtbl.address() - base);
			a_reason = located + ". Attached: making a skill legendary drops it to the level set on the Skills tab, or keeps it.";
			return true;
		}

		// ---- Legendary threshold / Legendary button ----------------------------------------------

		float* g_values = nullptr;   // [0] threshold, [1] button - what the two compares now read

		struct Site
		{
			const char* group;
			const char* what;
			const char* vanilla;        // what stays true while this group is not attached
			const char* attached;       // what attaching it changes
			const char* seSig;          // the shape (Kassent's 1.5.x anchor); the comiss sits at seAt
			std::ptrdiff_t seAt;
			std::size_t copies;         // how many copies of this check the game carries - every one is patched, or none
			const char* aeSig;          // an AE shape where one is known, else nullptr
			std::ptrdiff_t aeAt;
			std::uint64_t aeId;         // AE Address Library id + offset of `call [r+18h]; comiss` (DreadedAndy's listing)
			std::ptrdiff_t aeIdAt;
			int slot;
			std::uintptr_t comiss[2]{}; // the verified instructions, 0 until then
			std::size_t count = 0;
		};

		// The eligibility check exists TWICE in the 1.5.97 exe (measured in game 2026-09-11: the shape matched
		// in 2 places and the single-match rule refused it); the reference uncapper patched a second
		// "...Alt" site for the same reason. Patching one would let the key and the button disagree, so both
		// are proven and patched together. The actor-value-owner offset byte (B0 on SE, B8 on AE) is a
		// wildcard, so the same shape can find the AE copies.
		// A THIRD comparison against 100 sits inside the game's own "make legendary" answer: it fetches the
		// skill's base value, compares it with 100.0 and jumps past the reset when the skill is below it
		// (read from the running 1.5.97 game, 2026-09-11: `call [rax+18h]; comiss xmm0,[rip+..]; jb +85h`, the
		// jb being the instruction Kassent's reset hook replaced). With a threshold below 100 the skill could
		// be made legendary but kept its level - so it reads the threshold too.
		Site g_sites[3] = {
			{ "Legendary threshold", "the checks that decide whether a skill may be made legendary",
			  "A skill can be made legendary at 100, as in vanilla.",
			  "a skill can be made legendary from the level set on the Skills tab.",
			  "8B D0 48 8D 8F ?? 00 00 00 FF 53 18 0F 2F 05 ?? ?? ?? ?? 0F 82", 0x0C, 2,
			  nullptr, 0, 52520, 0x157, 0 },
			{ "Legendary button", "the check that decides whether the Skills menu shows the Legendary hint",
			  "The Skills menu shows the Legendary hint at 100, as in vanilla.",
			  "the Legendary hint follows the level set on the Skills tab, or stays hidden.",
			  "48 8B 0D ?? ?? ?? ?? 48 81 C1 B0 00 00 00 48 8B 01 8B D6 FF 50 18 0F 2F 05 ?? ?? ?? ?? 72", 0x16, 1,
			  "48 8B 0D ?? ?? ?? ?? 48 81 C1 B8 00 00 00 48 8B 01 41 8B D7 FF 50 18 0F 2F 05 ?? ?? ?? ?? 72", 0x17,
			  52527, 0x167, 1 },
			{ "Legendary reset threshold", "the check inside the game's own \"make legendary\" answer that skips the reset below 100",
			  "A skill made legendary below 100 keeps its level, as in vanilla.",
			  "a skill made legendary from the threshold set on the Skills tab is reset the way that tab says.",
			  "48 8B 01 8B 56 1C FF 50 18 0F 2F 05 ?? ?? ?? ?? 0F 82 85 00 00 00", 0x09, 1,
			  nullptr, 0, 52591, 0x1CD, 0 },
		};

		// `comiss xmm0, [rip+disp32]` whose operand is exactly 100.0. The bytes are checked before the
		// displacement is followed, so a mismatch never reads through a wrong pointer.
		bool IsComiss100(std::uintptr_t a_at)
		{
			const auto* b = reinterpret_cast<const std::uint8_t*>(a_at);
			if (b[0] != 0x0F || b[1] != 0x2F || b[2] != 0x05) { return false; }
			const auto disp = *reinterpret_cast<const std::int32_t*>(a_at + 3);
			return *reinterpret_cast<const float*>(a_at + 7 + static_cast<std::intptr_t>(disp)) == kVanilla;
		}

		// By shape first. The AE Address Library route is taken only when the player has opted in: an id
		// the database does not hold ends the game, and nobody should meet that with the feature off.
		std::vector<std::uintptr_t> Locate(const Site& a_site, bool a_allowId, std::string& a_how, std::string& a_note)
		{
			const auto hits = Signature::FindAll(a_site.seSig, a_site.seAt, 4);
			if (hits.size() == a_site.copies) { a_how = std::format("matched {} time(s) by shape, as expected", hits.size()); return hits; }
			a_note = std::format("its shape matched {} time(s) where {} {} expected", hits.size(), a_site.copies,
								 a_site.copies == 1 ? "is" : "are");
			if (a_site.aeSig)
			{
				const auto ae = Signature::FindAll(a_site.aeSig, a_site.aeAt, 4);
				if (ae.size() == a_site.copies) { a_how = "matched by its AE shape"; return ae; }
			}
#if RUNTIME_LINE != 17
			// The Address Library route names ONE site, so it is only taken for a check the game carries once:
			// patching one of two copies would let the key and the button disagree.
			if (a_allowId && REL::Module::IsAE() && a_site.copies == 1)
			{
				const auto fn = REL::ID(a_site.aeId).address();
				const auto* b = reinterpret_cast<const std::uint8_t*>(fn + a_site.aeIdAt);
				if (b[0] == 0xFF && b[2] == 0x18)
				{
					a_how = std::format("Address Library id {} + 0x{:X}", a_site.aeId, a_site.aeIdAt);
					return { fn + static_cast<std::uintptr_t>(a_site.aeIdAt) + 3 };
				}
				a_note = std::format("Address Library id {} + 0x{:X} does not hold `call [r+18h]` ({:02X} {:02X} {:02X})",
									 a_site.aeId, a_site.aeIdAt, b[0], b[1], b[2]);
			}
#else
			(void)a_allowId;
#endif
			return {};
		}

		bool InstallSite(Site& a_site, std::string& a_reason)
		{
			const std::string vanilla = std::string(" ") + a_site.vanilla;
			std::string how, note;
			const auto sites = Locate(a_site, settings::legendary::control, how, note);
			const auto base = REL::Module::get().base();
			if (sites.empty())
			{
				const bool uncapper = GetModuleHandleA("SkyrimUncapper.dll") != nullptr;
				a_reason = "refused: " + note + (uncapper ? ". Skyrim Skill Uncapper is loaded, and with its legendary settings on it patches this same place - "
															"use one mod's legendary settings, not both."
														  : "; nothing was written.") + vanilla;
				return false;
			}
			// Every copy is proven before any is written: all of them, or none.
			std::string offsets;
			for (const auto at : sites)
			{
				if (!IsComiss100(at))
				{
					const auto* b = reinterpret_cast<const std::uint8_t*>(at);
					a_reason = std::format("refused: game offset 0x{:X} ({}) does not hold `comiss xmm0, [rip+100.0]` ({:02X} {:02X} {:02X}); "
										   "nothing was written.{}", at - base, how, b[0], b[1], b[2], vanilla);
					return false;
				}
				offsets += std::format("{}0x{:X}", offsets.empty() ? "" : ", ", at - base);
			}
			a_site.count = 0;
			for (const auto at : sites) { if (a_site.count < std::size(a_site.comiss)) { a_site.comiss[a_site.count++] = at; } }
			const std::string located = std::format("verified at game offset{} {} ({}; each a comparison against the 100.0 constant) - {}",
													sites.size() > 1 ? "s" : "", offsets, how, a_site.what);
			if (!settings::legendary::control)
			{
				a_reason = located + ". Ready but not attached: Control legendary skills is off. Turn it on and restart to attach it." + vanilla;
				return false;
			}
			if (!g_values)
			{
				// Trampoline memory sits within 2 GB of the game, which a rip-relative operand needs. Aligned
				// by hand: earlier thunks leave the cursor on odd byte counts.
				auto* raw = static_cast<std::uint8_t*>(SKSE::GetTrampoline().allocate(3 * sizeof(float)));
				if (!raw) { a_reason = "refused: no trampoline space for the two values; nothing was written." + vanilla; return false; }
				g_values = reinterpret_cast<float*>((reinterpret_cast<std::uintptr_t>(raw) + 3) & ~static_cast<std::uintptr_t>(3));
				g_values[0] = kVanilla;
				g_values[1] = kVanilla;
			}
			float* value = g_values + a_site.slot;
			for (const auto at : sites)
			{
				const auto delta = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(value)) - static_cast<std::int64_t>(at + 7);
				if (delta < -0x80000000LL || delta > 0x7FFFFFFFLL)
				{
					a_reason = located + ". Refused: the value this mod would point it at is more than 2 GB away; nothing was written." + vanilla;
					return false;
				}
			}
			Refresh();
			for (const auto at : sites)
			{
				const auto disp = static_cast<std::int32_t>(static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(value)) - static_cast<std::int64_t>(at + 7));
				REL::safe_write(at + 3, &disp, sizeof(disp));
			}
			logger::info("{}: attached at game offset{} {} ({}); the comparison now reads this mod's value ({})", a_site.group,
						 sites.size() > 1 ? "s" : "", offsets, how, *value);
			a_reason = located + ". Attached: " + a_site.attached;
			return true;
		}

		class StatsSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && a_event->menuName == RE::StatsMenu::MENU_NAME) { Refresh(); }
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		StatsSink g_statsSink;
	}

	void Refresh()
	{
		if (!g_values) { return; }
		const bool on = settings::legendary::control;
		float t = settings::legendary::threshold;
		if (!(t >= 0.0F)) { t = kVanilla; }   // a NaN or negative from a hand-edited INI reads as vanilla
		const float threshold = on ? t : kVanilla;
		const float button = on ? (settings::legendary::hideButton ? FLT_MAX : t) : kVanilla;
		if (g_values[0] != threshold || g_values[1] != button)
		{
			logger::debug("legendary: the compares now read threshold {} and button {} (control {}, hide {})",
						  threshold, button, on, settings::legendary::hideButton);
		}
		g_values[0] = threshold;
		g_values[1] = button;
	}

	void Register()
	{
		Patches::Register(
			"Legendary reset level",
			"The level a skill drops to when it is made legendary, or whether it keeps its level.",
			InstallReset);
		Patches::Register(
			"Legendary threshold",
			"The skill level at which a skill can be made legendary.",
			[](std::string& a_reason) { return InstallSite(g_sites[0], a_reason); });
		Patches::Register(
			"Legendary button",
			"Whether the Skills menu shows the Legendary hint, and from which level.",
			[](std::string& a_reason) { return InstallSite(g_sites[1], a_reason); });
		Patches::Register(
			"Legendary reset threshold",
			"Whether a skill made legendary below 100 is reset (vanilla resets only from 100).",
			[](std::string& a_reason) { return InstallSite(g_sites[2], a_reason); });

		if (auto* ui = RE::UI::GetSingleton())
		{
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_statsSink);
		}
		else
		{
			logger::warn("legendary: the UI was not available at data load; settings changes apply after a restart instead of on the next Skills menu");
		}
	}

	std::string StatusJson()
	{
		auto* s = ResetSetting();
		const auto base = REL::Module::get().base();
		auto off = [base](std::uintptr_t a) { return a ? a - base : 0; };
		return std::format(
			R"({{"control":{},"threshold":{:.1f},"levelAfter":{:.1f},"keepLevel":{},"hideButton":{},"gameResetSetting":{:.1f},)"
			R"("resetHooked":{},"thresholdSite":"0x{:X}","thresholdCopies":{},"resetCheckSite":"0x{:X}","buttonSite":"0x{:X}","thresholdValue":{:g},"buttonValue":{:g},)"
			R"("runs":{},"lastSkill":{},"lastBefore":{:.1f},"lastWritten":{:.1f},"lastAfter":{:.1f}}})",
			settings::legendary::control, settings::legendary::threshold, settings::legendary::levelAfter,
			settings::legendary::keepLevel, settings::legendary::hideButton, s ? s->data.f : -1.0F,
			g_resetHooked.load(), off(g_sites[0].comiss[0]), g_sites[0].count, off(g_sites[2].comiss[0]), off(g_sites[1].comiss[0]),
			g_values ? g_values[0] : -1.0F, g_values ? g_values[1] : -1.0F,
			g_runs.load(), g_lastSkill.load(), g_lastBefore.load(), g_lastWritten.load(), g_lastAfter.load());
	}
}
