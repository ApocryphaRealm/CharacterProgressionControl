#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Compat.h"
#include "Enchanting.h"
#include "Levelling.h"
#include "Patches.h"
#include "Presets.h"
#include "SkillList.h"
#include "Skills.h"
#include "Settings.h"
#include "Signature.h"
#include "utils/Logger.h"

#include <cstdlib>
#include <format>
#include <string>
#include <string_view>

namespace DevBenchTool
{
	namespace
	{
		std::string EscapeJson(std::string_view a_in)
		{
			std::string out;
			out.reserve(a_in.size() + 8);
			for (const char c : a_in)
			{
				switch (c)
				{
				case '\\': out += "\\\\"; break;
				case '"': out += "\\\""; break;
				case '\n': out += "\\n"; break;
				default: out += c; break;
				}
			}
			return out;
		}

		// Pull a quoted string value out of the args JSON: ..."pattern":"48 8B 01 ??"...
		// Deliberately a small hand parser rather than a JSON dependency - the rest of this tool
		// already matches on substrings, and a signature is plain ASCII with no escaping.
		std::string ExtractString(std::string_view a_args, std::string_view a_key)
		{
			const std::string needle = std::string("\"") + std::string(a_key) + "\"";
			auto at = a_args.find(needle);
			if (at == std::string_view::npos) { return {}; }
			at = a_args.find(':', at + needle.size());
			if (at == std::string_view::npos) { return {}; }
			const auto open = a_args.find('"', at);
			if (open == std::string_view::npos) { return {}; }
			const auto close = a_args.find('"', open + 1);
			if (close == std::string_view::npos) { return {}; }
			return std::string(a_args.substr(open + 1, close - open - 1));
		}

		float ReadNumber(std::string_view a_args, std::string_view a_key)
		{
			const auto at = a_args.find(a_key);
			if (at == std::string_view::npos) { return 0.0F; }
			return std::strtof(std::string(a_args.substr(at + a_key.size(), 16)).c_str(), nullptr);
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_argsJson ? a_argsJson : "";

			if (args.find("\"reload\"") != std::string_view::npos)
			{
				const bool ok = settings::Reload();
				Levelling::RequestApply();
				Enchanting::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":{},"op":"reload"}})", ok ? "true" : "false").c_str());
				return;
			}
			if (args.find("\"apply\"") != std::string_view::npos)
			{
				Levelling::RequestApply();
				a_write(a_sink, R"({"ok":true,"op":"apply"})");
				return;
			}
			if (args.find("\"defaults\"") != std::string_view::npos)
			{
				settings::RestoreDefaults();
				Levelling::RequestApply();
				Enchanting::RequestApply();
				a_write(a_sink, R"({"ok":true,"op":"defaults"})");
				return;
			}
			// op=override:0|1 - turn the level-cost policy on or off.
			if (args.find("override:") != std::string_view::npos)
			{
				const bool on = ReadNumber(args, "override:") != 0.0F;
				settings::levelling::overrideCost = on;
				Levelling::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"override","value":{}}})", on ? "true" : "false").c_str());
				return;
			}
			// op=base:<n> / op=mult:<n> - a setting change; each applies immediately.
			for (const auto& [key, target] : { std::pair{ "base:", &settings::levelling::base },
											   std::pair{ "mult:", &settings::levelling::mult } })
			{
				if (args.find(key) != std::string_view::npos)
				{
					*target = ReadNumber(args, key);
					Levelling::RequestApply();
					a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.1f}}})", key, *target).c_str());
					return;
				}
			}

			// op=advance, "xp":<n>, "skill":<0..17> (0 = One-handed) - feed REAL skill experience through
			// the game's own AddSkillExperience, the exact path the skill-cap patch sits on. The game
			// levels the skill up repeatedly until it meets the cap, so one large amount is a clean test:
			// it stops at 100 with the patch off and passes it with the patch on. Testing only - it is
			// how the cap patch is watched working without a keyboard. Game-thread work, so queued.
			if (args.find("\"advance\"") != std::string_view::npos)
			{
				const float xp = ReadNumber(args, "\"xp\":");
				const int idx = static_cast<int>(ReadNumber(args, "\"skill\":"));
				if (xp <= 0.0F || idx < 0 || idx >= skilllist::kCount)
				{
					a_write(a_sink, R"({"ok":false,"op":"advance","error":"need \"xp\":<n> above 0 and \"skill\":<0..17>"})");
					return;
				}
				const auto av = static_cast<RE::ActorValue>(6 + idx);   // skill ids run 6..23, matching CPC_GetSkillCap
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddTask([av, xp]() {
						if (auto* player = RE::PlayerCharacter::GetSingleton()) { player->AddSkillExperience(av, xp); }
					});
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"advance","skill":"{}","xp":{:.1f},"queued":true}})",
											skilllist::kIniName[idx], xp).c_str());
				return;
			}

			if (args.find("\"skills\"") != std::string_view::npos)
			{
				const auto sk = Skills::GetState();
				// The ActorValue is what the cap check actually reads (the site calls GetBaseActorValue);
				// PlayerSkills' own `level` field lags it (measured 2026-09-05), so report both.
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* avo = player ? player->AsActorValueOwner() : nullptr;
				std::string rows;
				for (int i = 0; i < skilllist::kCount; ++i)
				{
					const auto av = static_cast<RE::ActorValue>(6 + i);
					const float base = avo ? avo->GetBaseActorValue(av) : 0.0F;
					const float current = avo ? avo->GetActorValue(av) : 0.0F;
					if (i) { rows += ","; }
					rows += std::format(
						R"({{"skill":"{}","base":{:.1f},"current":{:.1f},"level":{:.1f},"xp":{:.1f},"threshold":{:.1f},"cap":{:.1f},"formulaCap":{:.1f}}})",
						skilllist::kIniName[i], base, current, sk.skill[i].level, sk.skill[i].xp, sk.skill[i].levelThreshold,
						settings::skills::cap[i], settings::skills::formulaCap[i]);
				}
				a_write(a_sink, std::format(
					R"({{"ok":true,"op":"skills","readable":{},"capsActive":{},"overrideCaps":{},)"
					R"("characterLevel":{},"characterXp":{:.1f},"characterThreshold":{:.1f},"skills":[{}]}})",
					sk.readable ? "true" : "false",
					Patches::IsInstalled("Skill caps") ? "true" : "false",
					settings::skills::overrideCaps ? "true" : "false",
					sk.characterLevel, sk.characterXp, sk.characterThreshold, rows).c_str());
				return;
			}
			if (args.find("\"enchanting\"") != std::string_view::npos)
			{
				const auto& g = Enchanting::Settings();
				std::string rows;
				const auto& es = g.Entries();
				for (int i = 0; i < static_cast<int>(es.size()); ++i)
				{
					if (i) { rows += ","; }
					rows += std::format(R"({{"name":"{}","found":{},"vanilla":{:.4f},"configured":{:.4f},"live":{:.4f}}})",
										es[static_cast<std::size_t>(i)].name,
										es[static_cast<std::size_t>(i)].found ? "true" : "false",
										es[static_cast<std::size_t>(i)].vanilla,
										es[static_cast<std::size_t>(i)].value ? *es[static_cast<std::size_t>(i)].value : 0.0F,
										g.Live(i));
				}
				a_write(a_sink, std::format(
					R"({{"ok":true,"op":"enchanting","captured":{},"found":{},"overriding":{},"settings":[{}]}})",
					g.Captured() ? "true" : "false", g.FoundCount(),
					settings::enchanting::overrideCost ? "true" : "false", rows).c_str());
				return;
			}
			// op=scan with a "pattern" field - search the RUNNING game's executable section for a
			// byte signature. This exists because the shipped SkyrimSE.exe is Steam-packed: its
			// .text is encrypted on disk, so a signature can only ever be checked here, in the
			// decrypted image. Read-only, and it reports the match COUNT so an ambiguous
			// signature is visibly refused rather than silently taking the first hit.
			if (args.find("\"scan\"") != std::string_view::npos)
			{
				const auto pattern = ExtractString(args, "pattern");
				if (pattern.empty())
				{
					a_write(a_sink, R"({"ok":false,"op":"scan","error":"no \"pattern\" given"})");
					return;
				}
				std::uintptr_t base = 0;
				std::size_t size = 0;
				Signature::ModuleRange(base, size);
				const auto r = Signature::Find(pattern, 0);
				a_write(a_sink, std::format(
					R"({{"ok":true,"op":"scan","found":{},"matches":{},"address":"0x{:X}",)"
					R"("moduleOffset":"0x{:X}","textBase":"0x{:X}","textSize":{},"note":"{}"}})",
					r.found ? "true" : "false", r.matches, r.address,
					r.found ? (r.address - base) : 0, base, size, EscapeJson(r.note)).c_str());
				return;
			}
			if (args.find("\"presets\"") != std::string_view::npos)
			{
				Presets::Refresh();
				std::string rows;
				bool first = true;
				for (const auto& n : Presets::All())
				{
					if (!first) { rows += ","; }
					first = false;
					rows += std::format("\"{}\"", EscapeJson(n));
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"presets","current":"{}","dir":"{}","all":[{}]}})",
											EscapeJson(Presets::Current()), EscapeJson(Presets::Dir()), rows).c_str());
				return;
			}
			// op=preset:<name> - select a preset by name.
			if (args.find("preset:") != std::string_view::npos)
			{
				const auto at = args.find("preset:") + 7;
				auto rest = std::string(args.substr(at));
				const auto quote = rest.find('"');
				if (quote != std::string::npos) { rest = rest.substr(0, quote); }
				const bool ok = Presets::Select(rest);
				a_write(a_sink, std::format(R"({{"ok":{},"op":"preset","current":"{}"}})",
											ok ? "true" : "false", EscapeJson(Presets::Current())).c_str());
				return;
			}
			if (args.find("\"compat\"") != std::string_view::npos)
			{
				std::string rows;
				bool first = true;
				for (const auto& d : Compat::All())
				{
					if (!first) { rows += ","; }
					first = false;
					rows += std::format(R"({{"name":"{}","present":{},"consequence":"{}"}})",
										EscapeJson(d.name), d.present ? "true" : "false",
										EscapeJson(d.consequence));
				}
				a_write(a_sink, std::format(
					R"({{"ok":true,"op":"compat","altExperience":{},"customSkills":{},"detected":[{}]}})",
					Compat::AlternativeExperienceActive() ? "true" : "false",
					Compat::CustomSkillsFrameworkPresent() ? "true" : "false", rows).c_str());
				return;
			}
			if (args.find("\"patches\"") != std::string_view::npos)
			{
				std::string rows;
				bool first = true;
				for (const auto& g : Patches::All())
				{
					if (!first) { rows += ","; }
					first = false;
					rows += std::format(R"({{"name":"{}","installed":{},"status":"{}","touches":"{}"}})",
										EscapeJson(g.name), g.installed ? "true" : "false",
										EscapeJson(g.status), EscapeJson(g.touches));
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"patches","groups":[{}]}})", rows).c_str());
				return;
			}

			const auto s = Levelling::GetState();
			const std::string json = std::format(
				"{{\"ok\":true,"
				"\"settings\":{{\"overrideCost\":{},\"base\":{:.1f},\"mult\":{:.1f},\"logLevel\":{},\"iniPath\":\"{}\"}},"
				"\"runtime\":{{\"captured\":{},\"overriding\":{},\"vanillaBase\":{:.1f},\"vanillaMult\":{:.1f},"
				"\"liveBase\":{:.1f},\"liveMult\":{:.1f},\"playerLevel\":{},\"costThisLevel\":{:.1f},\"applications\":{}}}}}",
				settings::levelling::overrideCost ? "true" : "false",
				settings::levelling::base, settings::levelling::mult,
				settings::debug::logLevel, EscapeJson(settings::GetIniPath()),
				s.captured ? "true" : "false", s.overriding ? "true" : "false",
				s.vanillaBase, s.vanillaMult, s.liveBase, s.liveMult,
				s.playerLevel, s.costThisLevel, s.applications);
			a_write(a_sink, json.c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt)
			{
				logger::info("DevBench not detected; skipping the \"cpc.control\" tool");
			}
			else
			{
				logger::debug("DevBench not detected yet; will retry at the next message");
			}
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Character Progression Control live state: the level-cost settings, the "
			"values this install had before the mod, what the game is using now, and the cost of the "
			"player's current level. op=reload re-reads the INI, op=apply re-writes the policy, "
			"op=defaults restores, op=override:0|1, op=base:<n>, op=mult:<n>. op=skills reads every "
			"skill's level, experience and threshold plus the configured caps; op=enchanting reads the "
			"charge-cost settings; op=presets lists the presets and which one this character is on, and "
			"op=preset:<name> selects one; op=patches lists each engine patch group and whether it "
			"installed.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("cpc.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("Registered \"cpc.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
