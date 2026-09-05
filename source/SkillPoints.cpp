#include "PCH.h"

#include "SkillPoints.h"

#include "Patches.h"
#include "Settings.h"
#include "SkillExperience.h"
#include "SkillList.h"

#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <sstream>

namespace SkillPoints
{
	namespace
	{
		constexpr const char* kMenuPath = "_root.LevelUpMenu_mc.";
		constexpr const char* kEvent = "SSL_SkillsDistributionCompleted";   // the movie's own event name
		constexpr const char* kRefundEvent = "CPC_RefundSkillPoints";        // what a respec mod sends us
		constexpr int kSkillBase = 15;
		constexpr std::uint32_t kRecordVersion = 1;

		int g_bank = 0;
		std::uint16_t g_lastGranted = 0;
		std::atomic<bool> g_applying{ false };
		std::string g_menuStatus = "no level-up menu has opened yet";
		bool g_fed = false;   // the movie accepted the settings on the last open

		float Threshold(int a_index)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* skills = player ? player->GetPlayerRuntimeData().skills : nullptr;
			if (!skills || !skills->data) { return 0.0F; }
			return skills->data->skills[a_index].levelThreshold - skills->data->skills[a_index].xp;
		}

		// One skill level through the game's own improve path: the amount that lands exactly on the
		// threshold, expressed in the units the path multiplies (the AVIF's use mult / offset).
		bool IncreaseOnce(int a_index)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return false; }
			const auto av = static_cast<RE::ActorValue>(6 + a_index);
			auto* avo = player->AsActorValueOwner();
			const float before = avo ? avo->GetBaseActorValue(av) : 0.0F;
			float remaining = Threshold(a_index);
			if (remaining <= 0.0F) { remaining = 1.0F; }
			float amount = remaining * 1.002F + 0.01F;
#if RUNTIME_LINE == 17
			{
				if (auto* info = RE::ActorValueList::GetActorValueInfo(av); info && info->skill && info->skill->useMult != 0.0F)
#else
			if (auto* list = RE::ActorValueList::GetSingleton())
			{
				if (auto* info = list->GetActorValue(av); info && info->skill && info->skill->useMult != 0.0F)
#endif
				{
					amount = (amount - info->skill->offsetMult) / info->skill->useMult;
				}
			}
			if (amount <= 0.0F) { amount = 0.01F; }
			player->AddSkillExperience(av, amount);
			const float after = avo ? avo->GetBaseActorValue(av) : 0.0F;
			if (after <= before)
			{
				// rounding left it a hair short: a nudge of one percent of the threshold finishes it
				player->AddSkillExperience(av, std::max(0.01F, amount * 0.02F));
			}
			return avo ? avo->GetBaseActorValue(av) > before : true;
		}

		void FeedMenu()
		{
			g_fed = false;
			auto* ui = RE::UI::GetSingleton();
			auto menu = ui ? ui->GetMenu(RE::LevelUpMenu::MENU_NAME) : RE::GPtr<RE::IMenu>{};
			auto* movie = (menu && menu->uiMovie) ? menu->uiMovie.get() : nullptr;
			if (!movie) { g_menuStatus = "the level-up menu opened but its movie was not reachable"; logger::warn("skill points: {}", g_menuStatus); return; }
			RE::GFxValue result;
			// caps: this mod's own per-skill caps (what the Skills tab holds; 100 is vanilla)
			RE::GFxValue caps[skilllist::kCount];
			for (int i = 0; i < skilllist::kCount; ++i) { caps[i] = RE::GFxValue(static_cast<double>(settings::skills::cap[i])); }
			if (!movie->Invoke((std::string(kMenuPath) + "setSkillCaps").c_str(), &result, caps, skilllist::kCount))
			{
				g_menuStatus = "the level-up menu is not the skill-point one (no setSkillCaps) - install this mod's Interface\\levelupmenu.swf, or one of Static Skill Leveling Rewritten's skins";
				logger::warn("skill points: {}", g_menuStatus);
				return;
			}
			RE::GFxValue cfg[7] = { RE::GFxValue(-1.0), RE::GFxValue(static_cast<double>(settings::staticlevel::maxIncreasesPerSkill)), RE::GFxValue(static_cast<double>(g_bank)),
									RE::GFxValue(static_cast<double>(settings::staticlevel::cost[0])), RE::GFxValue(static_cast<double>(settings::staticlevel::cost[1])),
									RE::GFxValue(static_cast<double>(settings::staticlevel::cost[2])), RE::GFxValue(static_cast<double>(settings::staticlevel::cost[3])) };
			movie->Invoke((std::string(kMenuPath) + "setLevelingSettings").c_str(), &result, cfg, 7);
			// the player: the movie extends a form by id (SKSE's Scaleform extension) to read the skills
			RE::GFxValue form;
			movie->CreateObject(&form);
			form.SetMember("formId", RE::GFxValue(20.0));
			movie->Invoke((std::string(kMenuPath) + "setPlayer").c_str(), &result, &form, 1);
			g_fed = true;
			g_menuStatus = std::format("level-up menu fed: {} points in the bank, up to {} per skill, costs {}/{}/{}/{}", g_bank,
									   settings::staticlevel::maxIncreasesPerSkill, settings::staticlevel::cost[0], settings::staticlevel::cost[1],
									   settings::staticlevel::cost[2], settings::staticlevel::cost[3]);
			logger::info("skill points: {}", g_menuStatus);
		}

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event || !a_event->opening || a_event->menuName != RE::LevelUpMenu::MENU_NAME) { return RE::BSEventNotifyControl::kContinue; }
				if (!settings::staticlevel::pointsEnabled) { return RE::BSEventNotifyControl::kContinue; }
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player) { return RE::BSEventNotifyControl::kContinue; }
				const auto level = player->GetLevel();
				if (level > g_lastGranted)
				{
					const int granted = PointsForLevel(level);
					g_bank += granted;
					if (settings::staticlevel::pointsCap > 0) { g_bank = std::min(g_bank, settings::staticlevel::pointsCap); }
					g_lastGranted = level;
					logger::info("skill points: level {} grants {} points; bank {}", level, granted, g_bank);
				}
				FeedMenu();
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// A skill's untrained value, the same two readings Potion of Clarity takes: the player's own NPC
		// record and 15 plus the race's boost; the higher is right in every observed case.
		int StartingLevel(RE::PlayerCharacter* a_player, RE::ActorValue a_skill)
		{
			int fromRecord = kSkillBase;
			const std::uint32_t index = static_cast<std::uint32_t>(a_skill) - 6;
			if (auto* npc = a_player->GetActorBase(); npc && index < RE::TESNPC::Skills::kTotal) { fromRecord = static_cast<int>(npc->playerSkills.values[index]); }
			int fromRace = kSkillBase;
			if (auto* race = a_player->GetRace())
			{
				for (const auto& boost : race->data.skillBoosts) { if (boost.skill.get() == a_skill) { fromRace += static_cast<int>(boost.bonus); } }
			}
			return std::max(fromRecord, fromRace);
		}

		int CostForLevel(int a_level)
		{
			const int tier = a_level < 25 ? 0 : a_level < 50 ? 1 : a_level < 75 ? 2 : 3;
			return settings::staticlevel::cost[tier];
		}

		class ModSink : public RE::BSTEventSink<SKSE::ModCallbackEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
			{
				if (a_event && a_event->eventName == kRefundEvent)
				{
					if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([]() { RefundAll(); }); }
					return RE::BSEventNotifyControl::kContinue;
				}
				if (!a_event || a_event->eventName != kEvent) { return RE::BSEventNotifyControl::kContinue; }
				if (!settings::staticlevel::pointsEnabled) { return RE::BSEventNotifyControl::kContinue; }
				const std::string diffs = a_event->strArg.c_str();
				const int remaining = static_cast<int>(a_event->numArg);
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddTask([diffs, remaining]() { ApplyAllocation(diffs, remaining); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		MenuSink g_menuSink;
		ModSink g_modSink;

		bool Install(std::string& a_reason)
		{
			if (!settings::staticlevel::pointsEnabled)
			{
				a_reason = "ready but off: Use skill points is off. Turn it on and restart, and install the level-up menu file, "
						   "for skills to advance by points spent at level up.";
				return false;
			}
			if (auto* ui = RE::UI::GetSingleton()) { ui->AddEventSink(&g_menuSink); }
			if (auto* src = SKSE::GetModCallbackEventSource()) { src->AddEventSink(&g_modSink); }
			logger::info("Skill points: listening for the level-up menu and its allocation event; ordinary skill experience is not banked while this is on");
			const bool suppressed = SkillExperience::ImproveHookAttached();
			a_reason = std::string("on: each level up grants points spent in the level-up menu") +
					   (suppressed ? "; ordinary skill experience is not banked. "
								   : ". Ordinary skill experience is NOT suppressed on this build of the game (its skill-improve site is not "
									 "present), so skills also still advance by use. ") +
					   "Whether the installed level-up menu is the skill-point one shows here after the first level up.";
			if (!suppressed) { logger::warn("Skill points: the skill-improve site is not present in this build, so ordinary skill experience still banks alongside points"); }
			return true;
		}
	}

	bool Applying() { return g_applying.load(); }
	int Bank() { return g_bank; }
	std::uint16_t LastGrantedLevel() { return g_lastGranted; }
	const std::string& MenuStatus() { return g_menuStatus; }

	int PointsForLevel(std::uint16_t a_level)
	{
		const float raw = static_cast<float>(settings::staticlevel::pointsPerLevel) + settings::staticlevel::pointsLevelMult * static_cast<float>(a_level);
		return std::max(0, static_cast<int>(std::floor(raw)));
	}

	void Grant(int a_points)
	{
		g_bank = std::max(0, g_bank + a_points);
		logger::info("skill points: {} granted by the test tool; bank {}", a_points, g_bank);
	}

	bool ApplyAllocation(const std::string& a_diffs, int a_remaining)
	{
		std::vector<int> diffs;
		std::stringstream ss(a_diffs);
		std::string part;
		while (std::getline(ss, part, ';')) { diffs.push_back(part.empty() ? 0 : std::atoi(part.c_str())); }
		if (diffs.size() != skilllist::kCount)
		{
			logger::warn("skill points: allocation \"{}\" has {} entries, expected {}; nothing applied", a_diffs, diffs.size(), skilllist::kCount);
			return false;
		}
		g_applying = true;
		int applied = 0;
		for (int i = 0; i < skilllist::kCount; ++i)
		{
			for (int n = 0; n < diffs[i]; ++n)
			{
				if (IncreaseOnce(i)) { ++applied; }
				else { logger::warn("skill points: {} did not rise on increase {} of {}", skilllist::kIniName[i], n + 1, diffs[i]); }
			}
		}
		g_applying = false;
		g_bank = std::max(0, a_remaining);
		logger::info("skill points: applied {} increase(s) from the level-up menu; {} point(s) left in the bank", applied, g_bank);
		g_menuStatus = std::format("last level up: {} skill increase(s) applied, {} point(s) banked", applied, g_bank);
		return true;
	}

	int RefundAll()
	{
		if (!settings::staticlevel::pointsEnabled) { logger::info("skill points: a refund was asked for, but skill points are off; nothing to refund"); return 0; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* avo = player ? player->AsActorValueOwner() : nullptr;
		auto* skills = player ? player->GetPlayerRuntimeData().skills : nullptr;
		if (!player || !avo) { return 0; }
		int refund = 0, reset = 0;
		for (int i = 0; i < skilllist::kCount; ++i)
		{
			const auto av = static_cast<RE::ActorValue>(6 + i);
			const int start = StartingLevel(player, av);
			const int current = static_cast<int>(avo->GetBaseActorValue(av));
			if (current <= start) { continue; }
			int points = 0;
			for (int lvl = start; lvl < current; ++lvl) { points += CostForLevel(lvl); }
			avo->SetBaseActorValue(av, static_cast<float>(start));
			if (skills && skills->data)
			{
				// The game caches each skill's level and next-level threshold in its progression data and
				// only recomputes them when the skill LEVELS UP, so a reset must rewrite them: otherwise the
				// first level after a respec would still cost what the old, higher level cost. Threshold =
				// improveMult x level^fSkillUseCurve + improveOffset (the AVIF's AVSK block; measured 2026-09-05:
				// One-Handed 20 -> 688.7, 22 -> 829.4 with mult 2 and curve 1.95).
				auto& sd = skills->data->skills[i];
				sd.level = static_cast<float>(start);
				sd.xp = 0.0F;
				float curve = 1.95F;
				if (auto* gs = RE::GameSettingCollection::GetSingleton()) { if (auto* s = gs->GetSetting("fSkillUseCurve")) { curve = s->GetFloat(); } }
#if RUNTIME_LINE == 17
				auto* info = RE::ActorValueList::GetActorValueInfo(av);
#else
				auto* list = RE::ActorValueList::GetSingleton();
				auto* info = list ? list->GetActorValue(av) : nullptr;
#endif
				if (info && info->skill) { sd.levelThreshold = info->skill->improveMult * std::pow(static_cast<float>(start), curve) + info->skill->improveOffset; }
			}
			refund += points; ++reset;
			logger::info("skill points: {} {} -> {} refunds {} point(s)", skilllist::kIniName[i], current, start, points);
		}
		g_bank += refund;
		if (settings::staticlevel::pointsCap > 0) { g_bank = std::min(g_bank, settings::staticlevel::pointsCap); }
		g_menuStatus = std::format("respec: {} skill(s) reset, {} point(s) refunded; bank {}", reset, refund, g_bank);
		logger::info("skill points: {}", g_menuStatus);
		return refund;
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc || !a_intfc->OpenRecord(kRecord, kRecordVersion)) { logger::error("skill points: could not open the co-save record; the bank was not written"); return; }
		const std::int32_t bank = g_bank;
		const std::uint16_t last = g_lastGranted;
		a_intfc->WriteRecordData(&bank, sizeof(bank));
		a_intfc->WriteRecordData(&last, sizeof(last));
	}

	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length)
	{
		(void)a_version; (void)a_length;
		std::int32_t bank = 0; std::uint16_t last = 0;
		if (!a_intfc->ReadRecordData(&bank, sizeof(bank)) || !a_intfc->ReadRecordData(&last, sizeof(last))) { logger::warn("skill points: short co-save record; bank left at 0"); return; }
		g_bank = std::max(0, bank);
		g_lastGranted = last;
		logger::info("skill points: this character has {} point(s) banked; points last granted at level {}", g_bank, g_lastGranted);
	}

	void OnRevert()
	{
		g_bank = 0;
		g_lastGranted = 0;
		g_menuStatus = "no level-up menu has opened yet";
	}

	void Register()
	{
		Patches::Register(
			"Skill points",
			"Skills advance by points spent in the level-up menu instead of by use.",
			Install);
	}
}
