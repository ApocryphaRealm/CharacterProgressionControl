#include "PCH.h"

#include "CarryWeight.h"

#include "Compat.h"
#include "RaceShare.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <mutex>

namespace CarryWeight
{
	namespace
	{
		// 3 (1.1.10): the share is a permanent modifier. A 2 record's share went into the base, which the load has already
		// rebuilt from the race, so nothing of it is left: it is read as 0.
		constexpr std::uint32_t kRecordVersion = 3;

		std::atomic<bool> g_installed{ false };
		std::mutex g_stateLock;
		State g_state;

		// The net amount this mod has applied to the player's carry weight, mirrored in the
		// co-save. Only main-thread Apply() writes it.
		float g_applied = 0.0F;
		// The permanent value as it stood after the last apply, and whether the next apply is the first
		// after a load. The game keeps some permanent modifiers across a save and not others (carry
		// weight came back without this mod's share on 2026-09-05; health kept its), so the first apply
		// after a load asks which happened instead of trusting the applied amount blindly.
		float g_lastPermanent = 0.0F;
		bool g_checkAfterLoad = false;

		class LevelSink : public RE::BSTEventSink<RE::LevelIncrease::Event>
		{
		public:
			static LevelSink* GetSingleton()
			{
				static LevelSink singleton;
				return &singleton;
			}
			RE::BSEventNotifyControl ProcessEvent(const RE::LevelIncrease::Event* a_event, RE::BSTEventSource<RE::LevelIncrease::Event>*) override
			{
				if (a_event) { logger::debug("carry weight: level-up event (level {}) - applying", a_event->newLevel); RequestApply(); }
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// This mod's share goes into the PERMANENT MODIFIER, not the base (1.1.10). Vtable slot 05 - SE's
		// ActorValueOwner::ModActorValue, which CommonLibSSE-NG 7.x names ModBaseActorValue - changes the base, and the game
		// rebuilds the player's base carry weight from the race on every load (2026-09-05: the share came back missing;
		// Carryweight on Level Up 1.0.7: base 400 saved, 300 after the load), and the base is the race mod's number. Slot 06
		// with kPermanent - SE's RestoreActorValue, 7.x's ModActorValue(modifier, ...) - moves the permanent modifier.
		// To be proven in game on each line (the 1.1.10 test request).
		void Modify(RE::ActorValueOwner* a_owner, float a_delta)
		{
#if RUNTIME_LINE == 17
			a_owner->ModActorValue(RE::ACTOR_VALUE_MODIFIERS::kPermanent, RE::ActorValue::kCarryWeight, a_delta);
#else
			a_owner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIERS::kPermanent, RE::ActorValue::kCarryWeight, a_delta);
#endif
		}
	}

	bool Controlling()
	{
		return settings::carryweight::control && !Compat::CarryWeightOwnedElsewhere();
	}

	void Apply()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		// placed as well as loaded: at the main menu the singleton exists with no character (gate rule
		// or-player-actor-values-need-a-placed-player)
		if (!player || !player->Is3DLoaded() || !player->GetParentCell()) { logger::debug("carry weight: no loaded player yet"); return; }
		auto* owner = player->AsActorValueOwner();
		if (!owner) { logger::warn("carry weight: the player has no actor-value owner; nothing applied"); return; }

		State s;
		s.playerLevel = player->GetLevel();
		s.current = owner->GetActorValue(RE::ActorValue::kCarryWeight);
		s.permanent = owner->GetPermanentActorValue(RE::ActorValue::kCarryWeight);
		s.controlling = Controlling();
		if (g_checkAfterLoad)
		{
			g_checkAfterLoad = false;
			if (std::fabs(g_applied) > 0.01F && std::fabs(s.permanent - (g_lastPermanent - g_applied)) < 0.5F && std::fabs(s.permanent - g_lastPermanent) > 0.5F)
			{
				logger::info("carry weight: the game did not keep this mod's {:.1f} across the load (permanent {:.1f}, was {:.1f}) - starting from zero", g_applied, s.permanent, g_lastPermanent);
				g_applied = 0.0F;
			}
		}
		if (settings::carryweight::control && !s.controlling)
		{
			for (const auto& d : Compat::All())
			{
				if (d.present && d.name.find("Carry") != std::string::npos) { s.standDown = d.name + " is loaded and owns carry weight"; break; }
			}
			if (s.standDown.empty()) { s.standDown = "another carry-weight mod of ours is loaded and owns carry weight"; }
		}

		// 1.1.10 (include/RaceShare.h): this mod holds a SHARE of the permanent carry weight and moves only that.
		//   additive (default): share = offset + perLevel x (level - 1) on top of the race's carry weight and every other
		//                       mod's - an Altmer under Imperious keeps its 250 and gets the growth on top;
		//   our base values:    the total is starting + perLevel x (level - 1), whatever the race and others set;
		//   starting values off: share = perLevel x (level - 1) only;
		//   off (or standing down): share 0 - the net applied comes off.
		if (auto* race = player->GetRace()) { s.raceBase = race->data.baseCarryWeight; }
		s.base = owner->GetBaseActorValue(RE::ActorValue::kCarryWeight);
		raceshare::Carry c;
		c.on = s.controlling;
		c.startingValues = settings::race::startingValues;
		c.ownBase = settings::race::ownBase;
		c.offset = settings::carryweight::offset;
		c.starting = settings::carryweight::starting;
		c.perLevel = settings::carryweight::perLevel;
		c.level = s.playerLevel;
		const float share = raceshare::CarryShare(c, s.permanent, g_applied);
		const float delta = share - g_applied;
		s.target = s.permanent + delta;

		if (std::fabs(delta) > 0.01F)
		{
			Modify(owner, delta);
			g_applied = share;
			logger::info("carry weight {:.1f} -> {:.1f} (level {}, {}; this mod's share {:.1f}; race {:.1f}, base {:.1f})",
						 s.permanent, s.target, s.playerLevel,
						 !s.controlling ? std::string("control off - this mod's share taken away")
						 : !c.startingValues ? std::format("starting values off: per level {:.1f} only", c.perLevel)
						 : c.ownBase ? std::format("our base values: starting {:.1f}, per level {:.1f}", c.starting, c.perLevel)
									 : std::format("on top of the race: offset {:+.1f}, per level {:.1f}", c.offset, c.perLevel),
						 g_applied, s.raceBase, s.base);
			s.current = owner->GetActorValue(RE::ActorValue::kCarryWeight);
			s.permanent = owner->GetPermanentActorValue(RE::ActorValue::kCarryWeight);
		}
		else
		{
			logger::debug("carry weight: already {:.1f} at level {} ({})", s.permanent, s.playerLevel, s.controlling ? "controlling" : "not controlling");
		}
		s.applied = g_applied;
		g_lastPermanent = s.permanent;

		std::scoped_lock l(g_stateLock);
		s.applications = g_state.applications + 1;
		g_state = s;
	}

	void RequestApply()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(Apply); }
	}

	void Install()
	{
		if (g_installed.exchange(true)) { return; }
		if (auto* source = RE::LevelIncrease::GetEventSource())
		{
			source->AddEventSink(LevelSink::GetSingleton());
			logger::info("carry weight: level-up event sink registered (applies on load, level-up, setting change and Apply now - no background tick)");
		}
		else
		{
			logger::warn("carry weight: the SKSE LevelIncrease event source is unavailable - applies on load, setting change and Apply now only");
		}
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc || !a_intfc->OpenRecord(kRecord, kRecordVersion)) { logger::error("carry weight: could not open the co-save record; the applied amount was not written"); return; }
		a_intfc->WriteRecordData(&g_applied, sizeof(g_applied));
		a_intfc->WriteRecordData(&g_lastPermanent, sizeof(g_lastPermanent));
	}

	void ReadRecord(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length)
	{
		g_applied = 0.0F; g_lastPermanent = 0.0F; g_checkAfterLoad = false;
		if (a_version == 2)
		{
			logger::info("carry weight: the save is from 1.1.9 or older, which put this mod's share into the base carry weight - the game rebuilt that "
						 "from the race at this load, so the share starts from 0 and is applied once as a permanent modifier");
			return;
		}
		if (!a_intfc || a_version != kRecordVersion || a_length != 2 * sizeof(float)) { logger::warn("carry weight: co-save record version {} length {} not understood; applied left at 0", a_version, a_length); return; }
		float v[2]{};
		if (a_intfc->ReadRecordData(v, sizeof(v)) == sizeof(v)) { g_applied = v[0]; g_lastPermanent = v[1]; g_checkAfterLoad = true; }
		logger::debug("carry weight: co-save loaded, net applied {:.1f}, permanent then {:.1f}", g_applied, g_lastPermanent);
	}

	void OnRevert()
	{
		g_applied = 0.0F;
		g_lastPermanent = 0.0F;
		g_checkAfterLoad = false;
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
		return std::format(R"({{"control":{},"controlling":{},"standDown":"{}","ownBase":{},"startingValues":{},"offset":{:.1f},"starting":{:.1f},"perLevel":{:.1f},"playerLevel":{},"raceBase":{:.1f},"base":{:.1f},"target":{:.1f},"permanent":{:.1f},"current":{:.1f},"applied":{:.1f},"applications":{}}})",
						   settings::carryweight::control ? "true" : "false", s.controlling ? "true" : "false", s.standDown,
						   settings::race::ownBase ? "true" : "false", settings::race::startingValues ? "true" : "false", settings::carryweight::offset,
						   settings::carryweight::starting, settings::carryweight::perLevel, s.playerLevel, s.raceBase, s.base, s.target, s.permanent, s.current,
						   s.applied, s.applications);
	}
}
