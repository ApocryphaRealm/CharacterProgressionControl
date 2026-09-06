#include "PCH.h"

#include "CarryWeight.h"

#include "Compat.h"
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
		constexpr std::uint32_t kRecordVersion = 2;

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

		void Modify(RE::ActorValueOwner* a_owner, float a_delta)
		{
#if RUNTIME_LINE == 17
			a_owner->ModBaseActorValue(RE::ActorValue::kCarryWeight, a_delta);
#else
			a_owner->ModActorValue(RE::ActorValue::kCarryWeight, a_delta);
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
		if (!player || !player->Is3DLoaded()) { logger::debug("carry weight: no loaded player yet"); return; }
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

		// On: carry weight = starting + perLevel x (level - 1), for the CURRENT level.
		// Off (or standing down): whatever it would be without this mod - the net applied comes off.
		float delta = 0.0F;
		if (s.controlling)
		{
			s.target = settings::carryweight::starting
				+ static_cast<float>(std::max<int>(0, s.playerLevel - 1)) * settings::carryweight::perLevel;
			delta = s.target - s.permanent;
		}
		else
		{
			s.target = s.permanent - g_applied;
			delta = -g_applied;
		}

		if (std::fabs(delta) > 0.01F)
		{
			Modify(owner, delta);
			g_applied += delta;
			logger::info("carry weight {:.1f} -> {:.1f} (level {}, {}; net applied {:.1f})",
						 s.permanent, s.target, s.playerLevel,
						 s.controlling ? std::format("starting {:.1f}, per level {:.1f}", settings::carryweight::starting, settings::carryweight::perLevel)
									   : std::string("control off - this mod's share taken away"),
						 g_applied);
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
		return std::format(R"({{"control":{},"controlling":{},"standDown":"{}","starting":{:.1f},"perLevel":{:.1f},"playerLevel":{},"target":{:.1f},"permanent":{:.1f},"current":{:.1f},"applied":{:.1f},"applications":{}}})",
						   settings::carryweight::control ? "true" : "false", s.controlling ? "true" : "false", s.standDown,
						   settings::carryweight::starting, settings::carryweight::perLevel, s.playerLevel, s.target, s.permanent, s.current, s.applied, s.applications);
	}
}
