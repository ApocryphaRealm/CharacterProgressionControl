#include "PCH.h"

#include "ExperienceSources.h"

#include "Compat.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <atomic>
#include <format>
#include <mutex>

namespace ExperienceSources
{
	namespace
	{
		constexpr std::uint32_t kRecordVersion = 1;
		constexpr const char* kSourceName[kCount] = { "quest", "location", "cleared", "kill", "book" };

		std::atomic<bool> g_installed{ false };
		std::mutex g_lock;
		State g_state;

		// The game's own notice for a level-up becoming available, looked up once; a plain line
		// if this game has no string by that name.
		const char* LevelUpNotice()
		{
			static const char* s_text = nullptr;
			if (s_text) { return s_text; }
			if (auto* gs = RE::GameSettingCollection::GetSingleton())
			{
				for (const char* name : { "sLevelUpAvailable", "sLevelUp" })
				{
					if (auto* setting = gs->GetSetting(name); setting && setting->GetType() == RE::Setting::Type::kString && setting->GetString() && *setting->GetString())
					{
						s_text = setting->GetString();
						logger::debug("experience sources: level-up notice is the game's \"{}\": \"{}\"", name, s_text);
						return s_text;
					}
				}
			}
			s_text = "Level up available";
			return s_text;
		}

		float QuestAmount(const RE::TESQuest* a_quest, const char*& a_kind)
		{
			using namespace settings::experience;
			using T = RE::QUEST_DATA::Type;
			switch (a_quest->GetType())
			{
			case T::kMainQuest: a_kind = "main quest"; return questMain;
			case T::kMagesGuild: case T::kThievesGuild: case T::kDarkBrotherhood: case T::kCompanionsQuest:
			case T::kCivilWar: case T::kDLC01_Vampire: case T::kDLC02_Dragonborn: a_kind = "faction quest"; return questFaction;
			case T::kDaedric: a_kind = "Daedric quest"; return questDaedric;
			case T::kSideQuest: a_kind = "side quest"; return questSide;
			case T::kMiscellaneous: a_kind = "miscellaneous objective"; return questMisc;
			default: a_kind = "other quest"; return questOther;
			}
		}

		class QuestSink : public RE::BSTEventSink<RE::QuestStatus::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::QuestStatus::Event* a_event, RE::BSTEventSource<RE::QuestStatus::Event>*) override
			{
				if (!a_event || !a_event->quest || a_event->status != RE::QuestStatus::kCompleted) { return RE::BSEventNotifyControl::kContinue; }
				if (!Controlling()) { return RE::BSEventNotifyControl::kContinue; }
				const char* kind = "quest";
				const float amount = QuestAmount(a_event->quest, kind);
				const char* name = a_event->quest->GetFullName();
				Grant(kQuest, amount, std::format("{} completed: {}", kind, (name && *name) ? name : a_event->quest->GetFormEditorID()));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class LocationSink : public RE::BSTEventSink<RE::LocationDiscovery::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::LocationDiscovery::Event* a_event, RE::BSTEventSource<RE::LocationDiscovery::Event>*) override
			{
				if (!a_event || !Controlling()) { return RE::BSEventNotifyControl::kContinue; }
				const char* name = a_event->mapMarkerData ? a_event->mapMarkerData->locationName.GetFullName() : nullptr;
				Grant(kLocation, settings::experience::location, std::format("location discovered: {}", (name && *name) ? name : "(unnamed)"));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class ClearedSink : public RE::BSTEventSink<RE::LocationCleared::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::LocationCleared::Event*, RE::BSTEventSource<RE::LocationCleared::Event>*) override
			{
				if (!Controlling()) { return RE::BSEventNotifyControl::kContinue; }
				Grant(kCleared, settings::experience::cleared, "location cleared");
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class KillSink : public RE::BSTEventSink<RE::ActorKill::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::ActorKill::Event* a_event, RE::BSTEventSource<RE::ActorKill::Event>*) override
			{
				if (!a_event || !a_event->victim || !a_event->killer || !Controlling()) { return RE::BSEventNotifyControl::kContinue; }
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || a_event->victim == player) { return RE::BSEventNotifyControl::kContinue; }
				const bool byPlayer = a_event->killer == player;
				const bool byFollower = settings::experience::followerKills && (a_event->killer->IsPlayerTeammate() || a_event->killer->IsCommandedActor());
				if (!byPlayer && !byFollower) { return RE::BSEventNotifyControl::kContinue; }
				if (a_event->victim->IsPlayerTeammate()) { return RE::BSEventNotifyControl::kContinue; }
				const auto level = a_event->victim->GetLevel();
				const float amount = settings::experience::killBase + settings::experience::killPerLevel * static_cast<float>(level);
				const char* name = a_event->victim->GetName();
				Grant(kKill, amount, std::format("{} killed{}: {} (level {})", byPlayer ? "you" : "a follower", byPlayer ? "" : " for you", (name && *name) ? name : "(unnamed)", level));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class BookSink : public RE::BSTEventSink<RE::BooksRead::Event>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BooksRead::Event* a_event, RE::BSTEventSource<RE::BooksRead::Event>*) override
			{
				if (!a_event || !a_event->book || !Controlling()) { return RE::BSEventNotifyControl::kContinue; }
				const char* name = a_event->book->GetFullName();
				Grant(kBook, a_event->skillBook ? settings::experience::skillBook : settings::experience::book,
					  std::format("{} read: {}", a_event->skillBook ? "skill book" : "book", (name && *name) ? name : "(untitled)"));
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		QuestSink g_quest; LocationSink g_location; ClearedSink g_cleared; KillSink g_kill; BookSink g_book;
	}

	bool Controlling() { return settings::experience::enabled && !Compat::AlternativeExperienceActive(); }
	bool StandingDown() { return settings::experience::enabled && Compat::AlternativeExperienceActive(); }

	void Grant(Source a_source, float a_amount, const std::string& a_what)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* skills = player ? player->GetInfoRuntimeData().skills : nullptr;
		if (!skills || !skills->data) { logger::warn("experience sources: {} - no character data to credit; nothing granted", a_what); return; }
		if (a_amount <= 0.0F)
		{
			logger::debug("experience sources: {} - the amount for this source is 0; nothing granted", a_what);
			return;
		}
		const float before = skills->data->xp;
		const float threshold = skills->data->levelThreshold;
		skills->data->xp += a_amount;
		const bool nowAvailable = before < threshold && skills->data->xp >= threshold;
		logger::info("experience sources: {} -> +{:.0f} experience ({:.0f} -> {:.0f} of {:.0f}{})", a_what, a_amount, before, skills->data->xp, threshold, nowAvailable ? "; a level up is available" : "");
		if (nowAvailable)
		{
#if RUNTIME_LINE == 17
			RE::SendHUDMessage::ShowHUDMessage(LevelUpNotice(), nullptr, true);   // CommonLibSSE-NG 7.x has no RE::DebugNotification
#else
			RE::DebugNotification(LevelUpNotice());
#endif
		}
		std::scoped_lock l(g_lock);
		g_state.count[a_source] += 1;
		g_state.xp[a_source] += a_amount;
		g_state.last = std::format("{} (+{:.0f})", a_what, a_amount);
	}

	void Install()
	{
		if (g_installed.exchange(true)) { return; }
		int registered = 0;
		if (auto* s = RE::QuestStatus::GetEventSource()) { s->AddEventSink(&g_quest); ++registered; }
		if (auto* s = RE::LocationDiscovery::GetEventSource()) { s->AddEventSink(&g_location); ++registered; }
		if (auto* s = RE::LocationCleared::GetEventSource()) { s->AddEventSink(&g_cleared); ++registered; }
		if (auto* s = RE::ActorKill::GetEventSource()) { s->AddEventSink(&g_kill); ++registered; }
		if (auto* s = RE::BooksRead::GetEventSource()) { s->AddEventSink(&g_book); ++registered; }
		logger::info("experience sources: {} of 5 event sources registered (quests, locations, clearing, kills, books); {}",
					 registered, !settings::experience::enabled ? "off - nothing is granted" : Compat::AlternativeExperienceActive() ? "standing down - the Experience mod is loaded" : "on");
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc || !a_intfc->OpenRecord(kRecord, kRecordVersion)) { logger::error("experience sources: could not open the co-save record; the tallies were not written"); return; }
		std::scoped_lock l(g_lock);
		a_intfc->WriteRecordData(g_state.count, sizeof(g_state.count));
		a_intfc->WriteRecordData(g_state.xp, sizeof(g_state.xp));
	}

	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length)
	{
		OnRevert();
		constexpr std::uint32_t kLength = sizeof(std::uint32_t) * kCount + sizeof(float) * kCount;
		if (!a_intfc || a_version != kRecordVersion || a_length != kLength) { logger::warn("experience sources: co-save record version {} length {} not understood; the tallies start again", a_version, a_length); return; }
		std::uint32_t count[kCount]{}; float xp[kCount]{};
		if (!a_intfc->ReadRecordData(count, sizeof(count)) || !a_intfc->ReadRecordData(xp, sizeof(xp))) { logger::warn("experience sources: short co-save record; the tallies start again"); OnRevert(); return; }
		std::scoped_lock l(g_lock);
		for (int i = 0; i < kCount; ++i) { g_state.count[i] = count[i]; g_state.xp[i] = xp[i]; }
		logger::debug("experience sources: co-save loaded - quests {} ({:.0f}), locations {} ({:.0f}), cleared {} ({:.0f}), kills {} ({:.0f}), books {} ({:.0f})",
					  count[0], xp[0], count[1], xp[1], count[2], xp[2], count[3], xp[3], count[4], xp[4]);
	}

	void OnRevert()
	{
		std::scoped_lock l(g_lock);
		g_state = State{};
	}

	State GetState()
	{
		std::scoped_lock l(g_lock);
		return g_state;
	}

	std::string StatusJson()
	{
		const auto s = GetState();
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* skills = player ? player->GetInfoRuntimeData().skills : nullptr;
		const float xp = (skills && skills->data) ? skills->data->xp : 0.0F;
		const float threshold = (skills && skills->data) ? skills->data->levelThreshold : 0.0F;
		std::string rows;
		for (int i = 0; i < kCount; ++i) { rows += std::format(R"({}"{}":{{"count":{},"xp":{:.1f}}})", i ? "," : "", kSourceName[i], s.count[i], s.xp[i]); }
		std::string last;
		for (const char c : s.last) { if (c == '"' || c == '\\') { last += '\\'; } last += c; }
		return std::format(R"({{"enabled":{},"controlling":{},"standingDown":{},"skillsPay":{},"characterXp":{:.1f},"characterThreshold":{:.1f},"last":"{}",{}}})",
						   settings::experience::enabled ? "true" : "false", Controlling() ? "true" : "false", StandingDown() ? "true" : "false",
						   settings::experience::skillsPay ? "true" : "false", xp, threshold, last, rows);
	}
}
