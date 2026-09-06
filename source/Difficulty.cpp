#include "PCH.h"

#include "Difficulty.h"

#include "DifficultyValues.h"
#include "Presets.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <filesystem>
#include <format>

namespace Difficulty
{
	namespace
	{
		constexpr const char* kNames[kCount] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };

		// The difficulty whose preset is in use, or -1 when none has been applied yet this
		// session. Compared against the game's value on every Sync.
		int g_applied = -1;
		bool g_installed = false;

		// The game's Settings menu lives in the Journal Menu's System tab, so its closing is the
		// one moment a difficulty change can have just happened. Nothing else is watched.
		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event || a_event->opening) { return RE::BSEventNotifyControl::kContinue; }
				if (a_event->menuName != RE::JournalMenu::MENU_NAME) { return RE::BSEventNotifyControl::kContinue; }
				// The event arrives on the main thread; the preset switch touches settings and
				// re-applies game values, all of which are main-thread work already.
				DifficultyValues::OnMenuClosed();
				Sync("journal closed");
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		MenuSink g_menuSink;

		bool PresetExists(const std::string& a_name)
		{
			const auto path = std::filesystem::path(Presets::Dir()) / (a_name + ".ini");
			std::error_code ec;
			return std::filesystem::exists(path, ec);
		}

		// Writes the configuration in hand into the file for a_difficulty, creating it if the
		// difficulty has never been met. Returns what was done, for the log.
		const char* StoreInto(int a_difficulty)
		{
			const auto name = PresetNameFor(a_difficulty);
			if (Presets::Current() == name)
			{
				return Presets::SaveCurrent() ? "saved into its preset" : "could NOT be saved into its preset";
			}
			// The configuration in hand belongs to some other preset (or the built-in default):
			// it is what the player had while playing on this difficulty, so it becomes this
			// difficulty's file without disturbing the preset it came from.
			return settings::SaveTo((std::filesystem::path(Presets::Dir()) / (name + ".ini")).string())
					   ? "written as its preset" : "could NOT be written as its preset";
		}
	}

	int Current()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player)
		{
			return -1;   // main menu: no player yet, and no difficulty to follow
		}
		// The runtime-data accessor, never the member: the layout differs by runtime.
		const auto d = player->GetPlayerRuntimeData().difficulty;
		if (d < 0 || d >= kCount)
		{
			static bool s_warned = false;
			if (!s_warned) { s_warned = true; logger::warn("the game reports difficulty {}, outside 0..{} - treating it as none", d, kCount - 1); }
			return -1;
		}
		return static_cast<int>(d);
	}

	const char* NameOf(int a_difficulty)
	{
		return (a_difficulty >= 0 && a_difficulty < kCount) ? kNames[a_difficulty] : "none";
	}

	std::string PresetNameFor(int a_difficulty)
	{
		return std::string("Difficulty - ") + NameOf(a_difficulty);
	}

	void Install()
	{
		if (g_installed) { return; }
		if (auto* ui = RE::UI::GetSingleton())
		{
			ui->AddEventSink(&g_menuSink);
			g_installed = true;
			logger::info("difficulty: listening for the Journal Menu closing (the game's Settings live there); following is {}",
						 settings::difficulty::follow ? "ON" : "off");
		}
		else
		{
			logger::warn("difficulty: RE::UI is not available yet; the difficulty listener was not installed");
		}
	}

	void Sync(const char* a_reason)
	{
		if (!settings::difficulty::follow)
		{
			logger::debug("difficulty sync ({}): following is off; nothing to do", a_reason);
			return;
		}
		const int now = Current();
		if (now < 0)
		{
			logger::debug("difficulty sync ({}): no player yet; nothing to do", a_reason);
			return;
		}
		if (now == g_applied)
		{
			logger::debug("difficulty sync ({}): still {}; nothing to do", a_reason, NameOf(now));
			return;
		}

		// Leaving a difficulty: its file gets what the player had while on it.
		if (g_applied >= 0)
		{
			logger::info("difficulty {} -> {} ({}): the configuration in hand was {} for {}",
						 NameOf(g_applied), NameOf(now), a_reason, StoreInto(g_applied), NameOf(g_applied));
		}
		else
		{
			logger::info("difficulty is {} ({}); adopting its preset", NameOf(now), a_reason);
		}

		// Entering one: load its file, or make it from what is in hand the first time it is met.
		const auto name = PresetNameFor(now);
		Presets::Refresh();
		if (PresetExists(name))
		{
			if (!Presets::Select(name))
			{
				logger::warn("difficulty: preset \"{}\" could not be read; the built-in default is in use instead", name);
			}
		}
		else
		{
			if (Presets::Export(name))
			{
				logger::info("difficulty: \"{}\" did not exist yet - created from the current configuration and selected", name);
			}
			else
			{
				logger::warn("difficulty: could not create \"{}\"; the configuration in hand stays as it is", name);
			}
		}
		g_applied = now;
	}

	std::string OnFollowChanged()
	{
		if (settings::difficulty::follow)
		{
			g_applied = -1;   // forget, so the current difficulty is adopted rather than assumed
			Sync("following turned on");
			const int now = Current();
			return now < 0 ? "Following the game's difficulty. It takes effect once a character is loaded."
						   : std::format("Following the game's difficulty: now on {} -> \"{}\".", NameOf(now), Presets::Current());
		}
		g_applied = -1;
		return "No longer following the game's difficulty. The configuration in hand stays as it is.";
	}

	bool SetGameDifficulty(int a_difficulty)
	{
		if (a_difficulty < 0 || a_difficulty >= kCount) { return false; }
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return false; }
		player->GetPlayerRuntimeData().difficulty = a_difficulty;
		// The Difficulty tab's regeneration set follows the game's difficulty the same way the Settings menu path does.
		DifficultyValues::OnMenuClosed();
		// What the game's own Settings menu also does: keep the preference in step so the value
		// survives, rather than only the live copy.
		if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton())
		{
			if (auto* setting = prefs->GetSetting("iDifficulty:Gameplay"))
			{
				setting->data.i = a_difficulty;
			}
			else
			{
				logger::debug("difficulty: iDifficulty:Gameplay is not in the preference collection; only the live value was set");
			}
		}
		logger::info("difficulty set to {} over DevBench", NameOf(a_difficulty));
		Sync("set over DevBench");
		return true;
	}

	std::string StatusJson()
	{
		const int now = Current();
		return std::format(R"({{"follow":{},"difficulty":{},"name":"{}","applied":{},"preset":"{}","mapsTo":"{}","exists":{}}})",
						   settings::difficulty::follow ? "true" : "false", now, NameOf(now), g_applied,
						   Presets::Current(), now >= 0 ? PresetNameFor(now) : "",
						   (now >= 0 && PresetExists(PresetNameFor(now))) ? "true" : "false");
	}
}
