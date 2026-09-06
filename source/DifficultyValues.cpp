#include "PCH.h"

#include "DifficultyValues.h"

#include "Compat.h"
#include "Difficulty.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <array>
#include <format>

namespace DifficultyValues
{
	namespace
	{
		// The twelve damage multipliers with their real vanilla values (Custom Difficulty UI read
		// them out of the original mod's Papyrus source; the game confirms them at Init).
		struct DamageInfo { const char* name; float vanilla; RE::Setting* resolved = nullptr; };
		std::array<DamageInfo, kDifficulties> g_toPlayer = { {
			{ "fDiffMultHPToPCVE", 0.50F }, { "fDiffMultHPToPCE", 0.75F }, { "fDiffMultHPToPCN", 1.00F },
			{ "fDiffMultHPToPCH", 1.50F }, { "fDiffMultHPToPCVH", 2.00F }, { "fDiffMultHPToPCL", 3.00F } } };
		std::array<DamageInfo, kDifficulties> g_byPlayer = { {
			{ "fDiffMultHPByPCVE", 2.00F }, { "fDiffMultHPByPCE", 1.50F }, { "fDiffMultHPByPCN", 1.00F },
			{ "fDiffMultHPByPCH", 0.75F }, { "fDiffMultHPByPCVH", 0.50F }, { "fDiffMultHPByPCL", 0.25F } } };

		// The regeneration settings: one real GameSetting each, vanilla captured at Init.
		struct RegenInfo { const char* name; RE::Setting* resolved = nullptr; float vanilla = 0.0F; };
		std::array<RegenInfo, kRegenPerDifficulty> g_regen = { {
			{ "fCombatHealthRegenRateMult" }, { "fCombatMagickaRegenRateMult" }, { "fCombatStaminaRegenRateMult" },
			{ "fDamagedHealthRegenDelay" }, { "fDamagedMagickaRegenDelay" }, { "fDamagedStaminaRegenDelay" }, { "fDamagedAVRegenDelay" } } };
		std::array<RegenInfo, kRegenGlobal> g_global = { {
			{ "fHealthRegenDelayMax" }, { "fMagickaRegenDelayMax" }, { "fStaminaRegenDelayMax" },
			{ "fOutOfBreathStaminaRegenDelay" }, { "fEssentialDownCombatHealthRegenMult" } } };

		bool g_initialised = false;
		int g_lastApplied = -1;   // the difficulty the regeneration set was last written for
		bool g_standDownLogged = false;

		RE::Setting* Resolve(RE::GameSettingCollection* a_collection, const char* a_name)
		{
			auto* setting = a_collection->GetSetting(a_name);
			if (!setting) { logger::warn("difficulty values: GameSetting \"{}\" is not in this game - its control is dropped", a_name); return nullptr; }
			if (setting->GetType() != RE::Setting::Type::kFloat)
			{
				logger::warn("difficulty values: GameSetting \"{}\" is not a float (type {}) - refusing to touch it", a_name, static_cast<int>(setting->GetType()));
				return nullptr;
			}
			return setting;
		}

		// Any regeneration slot still at the sentinel takes the captured vanilla value - so a
		// fresh install (and Restore defaults) is identical on every difficulty until changed.
		void SeedUnset()
		{
			using namespace settings::regen;
			for (int i = 0; i < kRegenPerDifficulty; ++i)
			{
				if (!g_regen[i].resolved) { continue; }
				for (int d = 0; d < kDifficulties; ++d) { if (perDifficulty[i][d] == kUnset) { perDifficulty[i][d] = g_regen[i].vanilla; } }
			}
			for (int i = 0; i < kRegenGlobal; ++i)
			{
				if (g_global[i].resolved && global[i] == kUnset) { global[i] = g_global[i].vanilla; }
			}
		}

		// The difficulty the regeneration set is written for: the game's, or Adept before a
		// player exists (vanilla's starting difficulty).
		int DifficultyForRegen()
		{
			const int d = Difficulty::Current();
			return d < 0 ? 2 : d;
		}
	}

	void Init()
	{
		if (g_initialised) { return; }
		auto* collection = RE::GameSettingCollection::GetSingleton();
		if (!collection) { logger::error("difficulty values: the GameSettingCollection is not available; the Difficulty tab applies nothing"); return; }
		int missing = 0;
		for (auto& info : g_toPlayer) { info.resolved = Resolve(collection, info.name); if (!info.resolved) { ++missing; } }
		for (auto& info : g_byPlayer) { info.resolved = Resolve(collection, info.name); if (!info.resolved) { ++missing; } }
		for (auto& info : g_regen)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.vanilla = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" vanilla {:.3f}", info.name, info.vanilla); } else { ++missing; }
		}
		for (auto& info : g_global)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.vanilla = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" vanilla {:.3f}", info.name, info.vanilla); } else { ++missing; }
		}
		g_initialised = true;
		SeedUnset();
		logger::info("difficulty values: {} of {} GameSettings resolved; damage control {}, regeneration control {}{}",
					 kDifficulties * 2 + kRegenPerDifficulty + kRegenGlobal - missing, kDifficulties * 2 + kRegenPerDifficulty + kRegenGlobal,
					 settings::damage::control ? "on" : "off", settings::regen::control ? "on" : "off",
					 Compat::CustomDifficultyUIPresent() ? " - Custom Difficulty UI is loaded, so this tab stands down" : "");
		Apply();
	}

	bool StandingDown()
	{
		return (settings::damage::control || settings::regen::control) && Compat::CustomDifficultyUIPresent();
	}

	void Apply()
	{
		if (!g_initialised) { return; }
		SeedUnset();
		const int difficulty = DifficultyForRegen();
		if (Compat::CustomDifficultyUIPresent())
		{
			// The standalone owns every one of these values; writing even vanilla would fight it.
			if (!g_standDownLogged) { g_standDownLogged = true; logger::info("difficulty values: Custom Difficulty UI is loaded - nothing is written from the Difficulty tab"); }
			g_lastApplied = difficulty;
			return;
		}
		const bool damage = settings::damage::control;
		for (int d = 0; d < kDifficulties; ++d)
		{
			if (g_toPlayer[d].resolved) { g_toPlayer[d].resolved->data.f = damage ? settings::damage::toPlayer[d] : g_toPlayer[d].vanilla; }
			if (g_byPlayer[d].resolved) { g_byPlayer[d].resolved->data.f = damage ? settings::damage::byPlayer[d] : g_byPlayer[d].vanilla; }
		}
		const bool regen = settings::regen::control;
		for (int i = 0; i < kRegenPerDifficulty; ++i)
		{
			if (g_regen[i].resolved) { g_regen[i].resolved->data.f = regen ? settings::regen::perDifficulty[i][difficulty] : g_regen[i].vanilla; }
		}
		for (int i = 0; i < kRegenGlobal; ++i)
		{
			if (g_global[i].resolved) { g_global[i].resolved->data.f = regen ? settings::regen::global[i] : g_global[i].vanilla; }
		}
		g_lastApplied = difficulty;
		logger::debug("difficulty values applied for {}: damage {} (to you x{:.2f}, by you x{:.2f}); regeneration {} (combat health x{:.2f}, health delay {:.1f} s)",
					  Difficulty::NameOf(difficulty), damage ? "controlled" : "vanilla", LiveDamage(difficulty, true), LiveDamage(difficulty, false),
					  regen ? "controlled" : "vanilla", LiveRegen(0), LiveRegen(3));
	}

	void RequestApply()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(Apply); }
	}

	void OnMenuClosed()
	{
		if (!g_initialised) { return; }
		const int now = DifficultyForRegen();
		if (now != g_lastApplied)
		{
			logger::info("difficulty values: the game's difficulty is now {} (was {}); re-applying", Difficulty::NameOf(now), Difficulty::NameOf(g_lastApplied));
			Apply();
		}
	}

	int LastAppliedDifficulty() { return g_lastApplied; }

	const char* DamageSettingName(int a_difficulty, bool a_toPlayer)
	{
		if (a_difficulty < 0 || a_difficulty >= kDifficulties) { return "?"; }
		return a_toPlayer ? g_toPlayer[a_difficulty].name : g_byPlayer[a_difficulty].name;
	}
	const char* RegenSettingName(int a_index) { return (a_index >= 0 && a_index < kRegenPerDifficulty) ? g_regen[a_index].name : "?"; }
	const char* GlobalSettingName(int a_index) { return (a_index >= 0 && a_index < kRegenGlobal) ? g_global[a_index].name : "?"; }
	bool RegenResolved(int a_index) { return a_index >= 0 && a_index < kRegenPerDifficulty && g_regen[a_index].resolved != nullptr; }
	bool GlobalResolved(int a_index) { return a_index >= 0 && a_index < kRegenGlobal && g_global[a_index].resolved != nullptr; }
	float RegenVanilla(int a_index) { return RegenResolved(a_index) ? g_regen[a_index].vanilla : 0.0F; }
	float GlobalVanilla(int a_index) { return GlobalResolved(a_index) ? g_global[a_index].vanilla : 0.0F; }
	float LiveDamage(int a_difficulty, bool a_toPlayer)
	{
		if (a_difficulty < 0 || a_difficulty >= kDifficulties) { return 0.0F; }
		const auto* s = a_toPlayer ? g_toPlayer[a_difficulty].resolved : g_byPlayer[a_difficulty].resolved;
		return s ? s->GetFloat() : 0.0F;
	}
	float LiveRegen(int a_index) { return RegenResolved(a_index) ? g_regen[a_index].resolved->GetFloat() : 0.0F; }
	float LiveGlobal(int a_index) { return GlobalResolved(a_index) ? g_global[a_index].resolved->GetFloat() : 0.0F; }

	void CopyRegenToAll(int a_from)
	{
		if (a_from < 0 || a_from >= kDifficulties) { return; }
		for (int i = 0; i < kRegenPerDifficulty; ++i)
		{
			for (int d = 0; d < kDifficulties; ++d) { settings::regen::perDifficulty[i][d] = settings::regen::perDifficulty[i][a_from]; }
		}
		logger::info("difficulty values: every difficulty's regeneration set now matches {}", Difficulty::NameOf(a_from));
	}

	std::string StatusJson()
	{
		std::string damage, regen, global;
		for (int d = 0; d < kDifficulties; ++d)
		{
			damage += std::format(R"({}{{"difficulty":"{}","toPlayer":{:.3f},"byPlayer":{:.3f},"liveToPlayer":{:.3f},"liveByPlayer":{:.3f}}})",
								  d ? "," : "", Difficulty::NameOf(d), settings::damage::toPlayer[d], settings::damage::byPlayer[d], LiveDamage(d, true), LiveDamage(d, false));
		}
		for (int i = 0; i < kRegenPerDifficulty; ++i)
		{
			std::string per;
			for (int d = 0; d < kDifficulties; ++d) { per += std::format("{}{:.3f}", d ? "," : "", settings::regen::perDifficulty[i][d]); }
			regen += std::format(R"({}{{"name":"{}","resolved":{},"vanilla":{:.3f},"live":{:.3f},"perDifficulty":[{}]}})",
								 i ? "," : "", g_regen[i].name, RegenResolved(i) ? "true" : "false", RegenVanilla(i), LiveRegen(i), per);
		}
		for (int i = 0; i < kRegenGlobal; ++i)
		{
			global += std::format(R"({}{{"name":"{}","resolved":{},"vanilla":{:.3f},"live":{:.3f},"value":{:.3f}}})",
								  i ? "," : "", g_global[i].name, GlobalResolved(i) ? "true" : "false", GlobalVanilla(i), LiveGlobal(i), settings::regen::global[i]);
		}
		return std::format(R"({{"damageControl":{},"regenControl":{},"standingDown":{},"difficulty":{},"difficultyName":"{}","lastApplied":{},"damage":[{}],"regen":[{}],"global":[{}]}})",
						   settings::damage::control ? "true" : "false", settings::regen::control ? "true" : "false", StandingDown() ? "true" : "false",
						   Difficulty::Current(), Difficulty::CurrentName(), g_lastApplied, damage, regen, global);
	}
}
