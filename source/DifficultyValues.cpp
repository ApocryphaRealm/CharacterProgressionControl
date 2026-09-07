#include "PCH.h"

#include "DifficultyValues.h"

#include "Compat.h"
#include "Difficulty.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>

namespace DifficultyValues
{
	namespace
	{
		// The twelve damage multipliers. `vanilla` is Skyrim's own number (the slider default and the
		// help text); `loaded` is what THIS game held at kDataLoaded, after every plugin's records - the
		// value "off" leaves alone and an on->off flip restores (logic library 43).
		struct DamageInfo { const char* name; float vanilla; RE::Setting* resolved = nullptr; float loaded = 0.0F; };
		std::array<DamageInfo, kDifficulties> g_toPlayer = { {
			{ "fDiffMultHPToPCVE", 0.50F }, { "fDiffMultHPToPCE", 0.75F }, { "fDiffMultHPToPCN", 1.00F },
			{ "fDiffMultHPToPCH", 1.50F }, { "fDiffMultHPToPCVH", 2.00F }, { "fDiffMultHPToPCL", 3.00F } } };
		std::array<DamageInfo, kDifficulties> g_byPlayer = { {
			{ "fDiffMultHPByPCVE", 2.00F }, { "fDiffMultHPByPCE", 1.50F }, { "fDiffMultHPByPCN", 1.00F },
			{ "fDiffMultHPByPCH", 0.75F }, { "fDiffMultHPByPCVH", 0.50F }, { "fDiffMultHPByPCL", 0.25F } } };

		// The regeneration settings: one real GameSetting each, the loaded value captured at Init.
		struct RegenInfo { const char* name; RE::Setting* resolved = nullptr; float vanilla = 0.0F; };
		std::array<RegenInfo, kRegenPerDifficulty> g_regen = { {
			{ "fCombatHealthRegenRateMult" }, { "fCombatMagickaRegenRateMult" }, { "fCombatStaminaRegenRateMult" },
			{ "fDamagedHealthRegenDelay" }, { "fDamagedMagickaRegenDelay" }, { "fDamagedStaminaRegenDelay" }, { "fDamagedAVRegenDelay" } } };
		std::array<RegenInfo, kRegenGlobal> g_global = { {
			{ "fHealthRegenDelayMax" }, { "fMagickaRegenDelayMax" }, { "fStaminaRegenDelayMax" },
			{ "fOutOfBreathStaminaRegenDelay" }, { "fEssentialDownCombatHealthRegenMult" } } };

		// Blade and Blunt's published pairs (its ESP's GMST records, read 2026-09-06 under its open
		// permissions): damage to the player as vanilla, damage by the player gentler.
		constexpr float kBladeAndBluntTo[kDifficulties] = { 0.50F, 0.75F, 1.00F, 1.50F, 2.00F, 3.00F };
		constexpr float kBladeAndBluntBy[kDifficulties] = { 1.50F, 1.25F, 1.00F, 1.00F, 0.75F, 0.50F };

		bool g_initialised = false;
		int g_lastApplied = -1;        // the difficulty the regeneration set was last written for
		bool g_standDownLogged = false;
		bool g_wroteDamage = false;    // this process has written the twelve, so "off" puts the loaded values back once
		bool g_wroteRegen = false;
		std::atomic<bool> g_levelSinkInstalled{ false };
		int g_levelRuleLast = -1;      // what the level rule last settled on
		bool g_bbIniFound = false;
		bool g_bbLevelScaling = false;

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

		// Any regeneration slot still at the sentinel takes the loaded value - so a fresh install
		// (and Restore defaults) is identical on every difficulty until changed.
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

		// Blade and Blunt's DLL steps the twelve at levels 10..50 while bLevelBasedDifficulty is true
		// in its INI (the file it ships says true). The DLL is GPL and is not read; the INI is a
		// text file next to ours, and the page asks for false while the mod is present.
		void ReadBladeAndBluntIni()
		{
			g_bbIniFound = false;
			g_bbLevelScaling = false;
			std::error_code ec;
			const auto path = std::filesystem::current_path(ec) / "Data" / "SKSE" / "Plugins" / "BladeAndBlunt.ini";
			std::ifstream in(path);
			if (!in) { return; }
			std::string line;
			while (std::getline(in, line))
			{
				std::string lowered;
				for (const char c : line) { if (!std::isspace(static_cast<unsigned char>(c))) { lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c))); } }
				if (lowered.rfind("blevelbaseddifficulty=", 0) != 0) { continue; }
				g_bbIniFound = true;
				const auto value = lowered.substr(std::string("blevelbaseddifficulty=").size());
				g_bbLevelScaling = value.rfind("true", 0) == 0 || value.rfind("1", 0) == 0;
				return;
			}
		}

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
				if (a_event)
				{
					logger::debug("difficulty values: level-up event (level {})", a_event->newLevel);
					ApplyLevelRule("level up");
					// Queued, so it lands after every other plugin's handler of this same event.
					RequestApply();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		void InstallLevelSink()
		{
			if (g_levelSinkInstalled.exchange(true)) { return; }
			if (auto* source = RE::LevelIncrease::GetEventSource())
			{
				source->AddEventSink(LevelSink::GetSingleton());
				logger::info("difficulty values: level-up event sink registered (the level rule and a re-apply run on every level-up)");
			}
			else
			{
				logger::warn("difficulty values: the SKSE LevelIncrease event source is unavailable - the level rule runs on load only");
			}
		}
	}

	void Init()
	{
		if (g_initialised) { return; }
		auto* collection = RE::GameSettingCollection::GetSingleton();
		if (!collection) { logger::error("difficulty values: the GameSettingCollection is not available; the Difficulty tab applies nothing"); return; }
		int missing = 0;
		for (auto& info : g_toPlayer)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.loaded = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" loaded {:.3f} (vanilla {:.2f})", info.name, info.loaded, info.vanilla); } else { ++missing; }
		}
		for (auto& info : g_byPlayer)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.loaded = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" loaded {:.3f} (vanilla {:.2f})", info.name, info.loaded, info.vanilla); } else { ++missing; }
		}
		for (auto& info : g_regen)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.vanilla = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" loaded {:.3f}", info.name, info.vanilla); } else { ++missing; }
		}
		for (auto& info : g_global)
		{
			info.resolved = Resolve(collection, info.name);
			if (info.resolved) { info.vanilla = info.resolved->GetFloat(); logger::debug("difficulty values: \"{}\" loaded {:.3f}", info.name, info.vanilla); } else { ++missing; }
		}
		g_initialised = true;
		SeedUnset();
		ReadBladeAndBluntIni();
		const auto overhaul = OverhaulLoaded();
		logger::info("difficulty values: {} of {} GameSettings resolved; damage control {}{}, regeneration control {}, difficulty by level {}{}{}",
					 kDifficulties * 2 + kRegenPerDifficulty + kRegenGlobal - missing, kDifficulties * 2 + kRegenPerDifficulty + kRegenGlobal,
					 settings::damage::control ? "on" : "off", settings::damage::sharedPair ? " (one pair for every difficulty)" : "",
					 settings::regen::control ? "on" : "off", settings::bylevel::enabled ? "on" : "off",
					 overhaul.empty() ? "" : std::format("; {} is loaded - its values are the loaded values, and this tab writes last while a control is on", overhaul),
					 Compat::CustomDifficultyUIPresent() ? "; Custom Difficulty UI is loaded, so this tab stands down" : "");
		if (Compat::BladeAndBluntPresent())
		{
			if (!g_bbIniFound) { logger::info("difficulty values: BladeAndBlunt.ini was not found beside this mod's INI, so its bLevelBasedDifficulty could not be read"); }
			else if (g_bbLevelScaling) { logger::warn("difficulty values: BladeAndBlunt.ini has bLevelBasedDifficulty = true - its DLL will step the damage multipliers at levels 10 to 50 as well; set it to false while this tab controls damage"); }
			else { logger::info("difficulty values: BladeAndBlunt.ini has bLevelBasedDifficulty = false - its DLL leaves the damage multipliers alone"); }
		}
		InstallLevelSink();
		Apply();
	}

	bool StandingDown()
	{
		return (settings::damage::control || settings::regen::control || settings::bylevel::enabled) && Compat::CustomDifficultyUIPresent();
	}

	void Apply()
	{
		if (!g_initialised) { return; }
		SeedUnset();
		const int difficulty = DifficultyForRegen();
		if (Compat::CustomDifficultyUIPresent())
		{
			// The standalone owns every one of these values; writing even the loaded values would fight it.
			if (!g_standDownLogged) { g_standDownLogged = true; logger::info("difficulty values: Custom Difficulty UI is loaded - nothing is written from the Difficulty tab"); }
			g_lastApplied = difficulty;
			return;
		}
		const bool damage = settings::damage::control;
		if (damage)
		{
			for (int d = 0; d < kDifficulties; ++d)
			{
				const float to = settings::damage::sharedPair ? settings::damage::sharedToPlayer : settings::damage::toPlayer[d];
				const float by = settings::damage::sharedPair ? settings::damage::sharedByPlayer : settings::damage::byPlayer[d];
				if (g_toPlayer[d].resolved) { g_toPlayer[d].resolved->data.f = to; }
				if (g_byPlayer[d].resolved) { g_byPlayer[d].resolved->data.f = by; }
			}
			g_wroteDamage = true;
		}
		else if (g_wroteDamage)
		{
			// Off writes nothing - except once, to hand back what this game loaded with.
			for (int d = 0; d < kDifficulties; ++d)
			{
				if (g_toPlayer[d].resolved) { g_toPlayer[d].resolved->data.f = g_toPlayer[d].loaded; }
				if (g_byPlayer[d].resolved) { g_byPlayer[d].resolved->data.f = g_byPlayer[d].loaded; }
			}
			g_wroteDamage = false;
			logger::info("difficulty values: damage control is off - the loaded damage multipliers are back");
		}
		const bool regen = settings::regen::control;
		if (regen)
		{
			for (int i = 0; i < kRegenPerDifficulty; ++i)
			{
				if (g_regen[i].resolved) { g_regen[i].resolved->data.f = settings::regen::perDifficulty[i][difficulty]; }
			}
			for (int i = 0; i < kRegenGlobal; ++i)
			{
				if (g_global[i].resolved) { g_global[i].resolved->data.f = settings::regen::global[i]; }
			}
			g_wroteRegen = true;
		}
		else if (g_wroteRegen)
		{
			for (int i = 0; i < kRegenPerDifficulty; ++i) { if (g_regen[i].resolved) { g_regen[i].resolved->data.f = g_regen[i].vanilla; } }
			for (int i = 0; i < kRegenGlobal; ++i) { if (g_global[i].resolved) { g_global[i].resolved->data.f = g_global[i].vanilla; } }
			g_wroteRegen = false;
			logger::info("difficulty values: regeneration control is off - the loaded regeneration values are back");
		}
		g_lastApplied = difficulty;
		logger::debug("difficulty values applied for {}: damage {} (to you x{:.2f}, by you x{:.2f}); regeneration {} (combat health x{:.2f}, health delay {:.1f} s)",
					  Difficulty::NameOf(difficulty), damage ? (settings::damage::sharedPair ? "one pair" : "controlled") : "not written",
					  LiveDamage(difficulty, true), LiveDamage(difficulty, false),
					  regen ? "controlled" : "not written", LiveRegen(0), LiveRegen(3));
	}

	void RequestApply()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask(Apply); }
	}

	void OnGameLoaded()
	{
		ApplyLevelRule("save loaded");
		RequestApply();
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

	int LevelRuleTarget(int a_level)
	{
		int best = -1;
		for (int d = 0; d < kDifficulties; ++d)
		{
			const int from = static_cast<int>(settings::bylevel::levelFor[d]);
			if (from <= 0 || a_level < from) { continue; }
			// The highest threshold reached wins; on a tie the higher difficulty does.
			if (best < 0 || from >= static_cast<int>(settings::bylevel::levelFor[best])) { best = d; }
		}
		return best;
	}

	int ApplyLevelRule(const char* a_reason)
	{
		if (!settings::bylevel::enabled) { return -1; }
		if (Compat::CustomDifficultyUIPresent()) { return -1; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return -1; }
		const int level = static_cast<int>(player->GetLevel());
		const int target = LevelRuleTarget(level);
		if (target < 0)
		{
			logger::debug("difficulty by level ({}): level {} reaches no row of the table; the game's difficulty stays", a_reason, level);
			return -1;
		}
		const int now = Difficulty::Current();
		if (now == target)
		{
			logger::debug("difficulty by level ({}): level {} says {}, which the game already is", a_reason, level, Difficulty::NameOf(target));
			g_levelRuleLast = target;
			return target;
		}
		logger::info("difficulty by level ({}): level {} -> {} (the game was on {})", a_reason, level, Difficulty::NameOf(target), Difficulty::NameOf(now));
		Difficulty::SetGameDifficulty(target, a_reason);
		g_levelRuleLast = target;
		return target;
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
	float LoadedDamage(int a_difficulty, bool a_toPlayer)
	{
		if (a_difficulty < 0 || a_difficulty >= kDifficulties) { return 0.0F; }
		const auto& info = a_toPlayer ? g_toPlayer[a_difficulty] : g_byPlayer[a_difficulty];
		return info.resolved ? info.loaded : info.vanilla;
	}
	float LiveDamage(int a_difficulty, bool a_toPlayer)
	{
		if (a_difficulty < 0 || a_difficulty >= kDifficulties) { return 0.0F; }
		const auto* s = a_toPlayer ? g_toPlayer[a_difficulty].resolved : g_byPlayer[a_difficulty].resolved;
		return s ? s->GetFloat() : 0.0F;
	}
	float LiveRegen(int a_index) { return RegenResolved(a_index) ? g_regen[a_index].resolved->GetFloat() : 0.0F; }
	float LiveGlobal(int a_index) { return GlobalResolved(a_index) ? g_global[a_index].resolved->GetFloat() : 0.0F; }

	void UseLoadedValues()
	{
		for (int d = 0; d < kDifficulties; ++d) { settings::damage::toPlayer[d] = LoadedDamage(d, true); settings::damage::byPlayer[d] = LoadedDamage(d, false); }
		logger::info("difficulty values: the damage table now holds the values this game loaded with");
	}
	void UseVanillaValues()
	{
		for (int d = 0; d < kDifficulties; ++d) { settings::damage::toPlayer[d] = g_toPlayer[d].vanilla; settings::damage::byPlayer[d] = g_byPlayer[d].vanilla; }
		logger::info("difficulty values: the damage table now holds Skyrim's vanilla values");
	}
	void UseBladeAndBlunt()
	{
		for (int d = 0; d < kDifficulties; ++d) { settings::damage::toPlayer[d] = kBladeAndBluntTo[d]; settings::damage::byPlayer[d] = kBladeAndBluntBy[d]; }
		logger::info("difficulty values: the damage table now holds Blade and Blunt's values");
	}
	void UseRequiem()
	{
		for (int d = 0; d < kDifficulties; ++d) { settings::damage::toPlayer[d] = 1.0F; settings::damage::byPlayer[d] = 1.0F; }
		logger::info("difficulty values: the damage table now holds Requiem's values (every multiplier 1.0)");
	}

	std::string OverhaulLoaded()
	{
		std::string out;
		if (Compat::BladeAndBluntPresent()) { out = "Blade and Blunt"; }
		if (Compat::RequiemPresent()) { out += out.empty() ? "Requiem" : " and Requiem"; }
		return out;
	}
	bool BladeAndBluntLevelScaling(bool& a_found)
	{
		a_found = g_bbIniFound;
		return g_bbLevelScaling;
	}

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
		std::string damage, regen, global, levels;
		for (int d = 0; d < kDifficulties; ++d)
		{
			damage += std::format(R"({}{{"difficulty":"{}","toPlayer":{:.3f},"byPlayer":{:.3f},"loadedToPlayer":{:.3f},"loadedByPlayer":{:.3f},"liveToPlayer":{:.3f},"liveByPlayer":{:.3f}}})",
								  d ? "," : "", Difficulty::NameOf(d), settings::damage::toPlayer[d], settings::damage::byPlayer[d],
								  LoadedDamage(d, true), LoadedDamage(d, false), LiveDamage(d, true), LiveDamage(d, false));
			levels += std::format("{}{}", d ? "," : "", settings::bylevel::levelFor[d]);
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
		auto* player = RE::PlayerCharacter::GetSingleton();
		const int level = player ? static_cast<int>(player->GetLevel()) : -1;
		bool bbFound = false;
		const bool bbScaling = BladeAndBluntLevelScaling(bbFound);
		return std::format(R"({{"damageControl":{},"sharedPair":{},"sharedToPlayer":{:.3f},"sharedByPlayer":{:.3f},"regenControl":{},"byLevel":{},"levelFor":[{}],"playerLevel":{},"levelTarget":{},"levelRuleLast":{},"wroteDamage":{},"wroteRegen":{},"standingDown":{},"difficulty":{},"difficultyName":"{}","lastApplied":{},"overhaul":"{}","bladeAndBlunt":{},"requiem":{},"bbIniFound":{},"bbLevelScaling":{},"damage":[{}],"regen":[{}],"global":[{}]}})",
						   settings::damage::control ? "true" : "false", settings::damage::sharedPair ? "true" : "false", settings::damage::sharedToPlayer, settings::damage::sharedByPlayer,
						   settings::regen::control ? "true" : "false", settings::bylevel::enabled ? "true" : "false", levels, level, level >= 0 ? LevelRuleTarget(level) : -1, g_levelRuleLast,
						   g_wroteDamage ? "true" : "false", g_wroteRegen ? "true" : "false", StandingDown() ? "true" : "false",
						   Difficulty::Current(), Difficulty::CurrentName(), g_lastApplied, OverhaulLoaded(),
						   Compat::BladeAndBluntPresent() ? "true" : "false", Compat::RequiemPresent() ? "true" : "false",
						   bbFound ? "true" : "false", bbScaling ? "true" : "false", damage, regen, global);
	}
}
