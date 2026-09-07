#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Compat.h"
#include "Enchanting.h"
#include "ExperienceSources.h"
#include "Levelling.h"
#include "Patches.h"
#include "Difficulty.h"
#include "DifficultyValues.h"
#include "Presets.h"
#include "SkillList.h"
#include "Skills.h"
#include "Attributes.h"
#include "CarryWeight.h"
#include "SkillPoints.h"
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
			if (args.find("\"save\"") != std::string_view::npos)
			{
				const bool ok = settings::Save();
				a_write(a_sink, std::format(R"({{"ok":{},"op":"save"}})", ok ? "true" : "false").c_str());
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
			// "levelup":"health|magicka|stamina" - presses that button on the game's own LevelUp Menu
			// (testing): the menu's button handler is invoked on its movie, exactly what a click does.
			if (const auto at = args.find("\"levelup\":\""); at != std::string_view::npos)
			{
				const auto start = at + 11;
				const auto end = args.find('"', start);
				const std::string which(args.substr(start, end == std::string_view::npos ? 0 : end - start));
				const std::uint32_t av = which == "health" ? 0x18 : which == "magicka" ? 0x19 : which == "stamina" ? 0x1A : 0;
				if (!av) { a_write(a_sink, R"({"ok":false,"op":"levelup","error":"health|magicka|stamina"})"); return; }
				const auto choose = Attributes::ChooseAddress();
				if (!choose) { a_write(a_sink, R"({"ok":false,"op":"levelup","error":"choose-attribute function not resolved"})"); return; }
				const char* fn = "(direct)";
				auto* ui = RE::UI::GetSingleton();
				auto menu = ui ? ui->GetMenu(RE::LevelUpMenu::MENU_NAME) : RE::GPtr<RE::IMenu>{};
				if (!menu || !menu->uiMovie) { a_write(a_sink, R"({"ok":false,"op":"levelup","error":"LevelUp Menu is not open"})"); return; }
				const std::string fnName = fn;
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddUITask([av, choose]() {
						auto* ui2 = RE::UI::GetSingleton();
						auto m = ui2 ? ui2->GetMenu(RE::LevelUpMenu::MENU_NAME) : RE::GPtr<RE::IMenu>{};
						if (!m) { logger::warn("levelup op: menu gone before the UI task ran"); return; }
						// exactly what the menu's addHealth/addMagicka/addStamina delegate handlers do
						using Choose_t = void(RE::IMenu*, std::uint32_t);
						reinterpret_cast<Choose_t*>(choose)(m.get(), av);
						logger::info("levelup op: called the choose-attribute function with av 0x{:X}", av);
					});
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"levelup","invoke":"{}","queued":true}})", fnName).c_str());
				return;
			}
			// "msgbox":<button index> - presses that button on the game's open message box (testing):
			// the same GameDelegate call the box's own buttons make.
			if (args.find("\"msgbox\":") != std::string_view::npos)
			{
				const int button = static_cast<int>(ReadNumber(args, "\"msgbox\":"));
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddUITask([button]() {
						auto* ui2 = RE::UI::GetSingleton();
						auto m = ui2 ? ui2->GetMenu(RE::MessageBoxMenu::MENU_NAME) : RE::GPtr<RE::IMenu>{};
						if (!m || !m->uiMovie) { logger::warn("msgbox op: no message box open"); return; }
						RE::GFxValue arr; m->uiMovie->CreateArray(&arr);
						RE::GFxValue idx(static_cast<double>(button)); arr.PushBack(idx);
						RE::GFxValue fnArgs[2]; fnArgs[0] = RE::GFxValue("buttonPress"); fnArgs[1] = arr;
						RE::GFxValue result;
						const bool ok = m->uiMovie->Invoke("_global.gfx.io.GameDelegate.call", &result, fnArgs, 2);
						logger::info("msgbox op: GameDelegate.call(buttonPress, {}) -> {}", button, ok);
					});
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"msgbox","button":{},"queued":true}})", button).c_str());
				return;
			}
			// "points":1 - the skill-point state; "grantpoints":<n> banks points; "allocate":"n0;..;n17" with
			// "remaining":<n> applies an allocation exactly as the level-up menu's event would (testing).
			if (args.find("\"points\"") != std::string_view::npos)
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"points","enabled":{},"bank":{},"lastGrantedLevel":{},"nextGrant":{},"status":"{}"}})",
					settings::staticlevel::pointsEnabled, SkillPoints::Bank(), SkillPoints::LastGrantedLevel(),
					SkillPoints::PointsForLevel(static_cast<std::uint16_t>((player ? player->GetLevel() : 0) + 1)), EscapeJson(SkillPoints::MenuStatus())).c_str());
				return;
			}
			if (args.find("\"refund\"") != std::string_view::npos)
			{
				if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([]() { SkillPoints::RefundAll(); }); }
				a_write(a_sink, R"({"ok":true,"op":"refund","queued":true})");
				return;
			}
			if (args.find("\"grantpoints\":") != std::string_view::npos)
			{
				SkillPoints::Grant(static_cast<int>(ReadNumber(args, "\"grantpoints\":")));
				a_write(a_sink, std::format(R"({{"ok":true,"op":"grantpoints","bank":{}}})", SkillPoints::Bank()).c_str());
				return;
			}
			if (const auto at = args.find("\"allocate\":\""); at != std::string_view::npos)
			{
				const auto start = at + 12;
				const auto end = args.find('"', start);
				const std::string diffs(args.substr(start, end == std::string_view::npos ? 0 : end - start));
				const int remaining = static_cast<int>(ReadNumber(args, "\"remaining\":"));
				if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([diffs, remaining]() { SkillPoints::ApplyAllocation(diffs, remaining); }); }
				a_write(a_sink, std::format(R"({{"ok":true,"op":"allocate","diffs":"{}","remaining":{},"queued":true}})", diffs, remaining).c_str());
				return;
			}
			// "swf":"<menu>|<path>" - invokes an ActionScript function on an open menu's movie, no arguments
			// (testing): e.g. "LevelUp Menu|_root.LevelUpMenu_mc.onehanded.skillIncrease".
			if (const auto at = args.find("\"swf\":\""); at != std::string_view::npos)
			{
				const auto start = at + 7;
				const auto end = args.find('"', start);
				const std::string spec(args.substr(start, end == std::string_view::npos ? 0 : end - start));
				const auto bar = spec.find('|');
				if (bar == std::string::npos) { a_write(a_sink, R"({"ok":false,"op":"swf","error":"need menu|path"})"); return; }
				const std::string menuName = spec.substr(0, bar), path = spec.substr(bar + 1);
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddUITask([menuName, path]() {
						auto* ui2 = RE::UI::GetSingleton();
						auto m = ui2 ? ui2->GetMenu(menuName) : RE::GPtr<RE::IMenu>{};
						if (!m || !m->uiMovie) { logger::warn("swf op: menu {} not open", menuName); return; }
						RE::GFxValue result;
						const bool ok = m->uiMovie->Invoke(path.c_str(), &result, nullptr, 0);
						logger::info("swf op: {} on {} -> {}", path, menuName, ok);
					});
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"swf","menu":"{}","path":"{}","queued":true}})", menuName, path).c_str());
				return;
			}
			// "peek":<game offset>, "count":<n> - hex bytes of the game's code (testing).
			if (args.find("\"peek\":") != std::string_view::npos)
			{
				const auto off = static_cast<std::uintptr_t>(ReadNumber(args, "\"peek\":"));
				int count = static_cast<int>(ReadNumber(args, "\"count\":")); if (count <= 0 || count > 256) { count = 16; }
				const auto* b = reinterpret_cast<const std::uint8_t*>(REL::Module::get().base() + off);
				std::string hex; for (int i = 0; i < count; ++i) { hex += std::format("{:02X} ", b[i]); }
				a_write(a_sink, std::format(R"({{"ok":true,"op":"peek","offset":"0x{:X}","bytes":"{}"}})", off, hex).c_str());
				return;
			}
			// "givexp":<n> - adds character experience directly (testing), so a level up is available
			// without grinding skills.
			if (args.find("\"givexp\":") != std::string_view::npos)
			{
				const float xp = ReadNumber(args, "\"givexp\":");
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* skills = player ? player->GetPlayerRuntimeData().skills : nullptr;
				if (!skills || !skills->data) { a_write(a_sink, R"({"ok":false,"op":"givexp","error":"no character loaded"})"); return; }
				skills->data->xp += xp;
				a_write(a_sink, std::format(R"({{"ok":true,"op":"givexp","xp":{:.1f},"threshold":{:.1f}}})", skills->data->xp, skills->data->levelThreshold).c_str());
				return;
			}
			// "static":0|1 and "peruse":<pct> - static levelling live, every skill (testing).
			if (args.find("\"static\":") != std::string_view::npos)
			{
				settings::staticlevel::enabled = ReadNumber(args, "\"static\":") != 0.0F;
				if (args.find("\"peruse\":") != std::string_view::npos)
				{
					const float pct = ReadNumber(args, "\"peruse\":");
					for (int i = 0; i < skilllist::kCount; ++i) { settings::staticlevel::xpPerUse[i] = pct; }
				}
				a_write(a_sink, std::format(R"({{"ok":true,"op":"static","enabled":{},"perUseOneHanded":{:.2f}}})", settings::staticlevel::enabled, settings::staticlevel::xpPerUse[0]).c_str());
				return;
			}
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

			// "tolevel":<n> - the skill-increase -> level multiplier, live (testing). "rate":<n> with
			// "skill":<i> - that skill's experience rate, live (testing).
			if (args.find("\"tolevel\":") != std::string_view::npos)
			{
				settings::skillexp::toLevelMult = ReadNumber(args, "\"tolevel\":");
				a_write(a_sink, std::format(R"({{"ok":true,"op":"tolevel","value":{:.2f}}})", settings::skillexp::toLevelMult).c_str());
				return;
			}
			if (args.find("\"rate\":") != std::string_view::npos)
			{
				const int idx = static_cast<int>(ReadNumber(args, "\"skill\":"));
				if (idx < 0 || idx >= skilllist::kCount) { a_write(a_sink, R"({"ok":false,"op":"rate","error":"skill index 0..17"})"); return; }
				settings::skillexp::mult[idx] = ReadNumber(args, "\"rate\":");
				a_write(a_sink, std::format(R"({{"ok":true,"op":"rate","skill":"{}","value":{:.2f}}})", skilllist::kIniName[idx], settings::skillexp::mult[idx]).c_str());
				return;
			}

			// "perktable":"1:1,20:2" - replace the perk table live (testing).
			if (const auto at = args.find("\"perktable\":\""); at != std::string_view::npos)
			{
				const auto start = at + 13;
				const auto end = args.find('"', start);
				const std::string text(args.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
				const bool ok = settings::levelup::ParsePerksTable(text);
				a_write(a_sink, std::format(R"({{"ok":{},"op":"perktable","table":"{}"}})", ok ? "true" : "false", EscapeJson(settings::levelup::PerksTableText())).c_str());
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
				const int perkPoints = player ? player->GetGameStatsData().perkCount : -1;
				auto av = [&](RE::ActorValue a) { return avo ? avo->GetBaseActorValue(a) : 0.0F; };
				auto pv = [&](RE::ActorValue a) { return avo ? avo->GetActorValue(a) : 0.0F; };   // current (temporary included)
				const std::string attrs = std::format(R"("health":{:.1f},"magicka":{:.1f},"stamina":{:.1f},"carryWeight":{:.1f},"healthPerm":{:.1f},"magickaPerm":{:.1f},"staminaPerm":{:.1f},"carryWeightPerm":{:.1f})",
					av(RE::ActorValue::kHealth), av(RE::ActorValue::kMagicka), av(RE::ActorValue::kStamina), av(RE::ActorValue::kCarryWeight),
					pv(RE::ActorValue::kHealth), pv(RE::ActorValue::kMagicka), pv(RE::ActorValue::kStamina), pv(RE::ActorValue::kCarryWeight));
				a_write(a_sink, std::format(
					R"({{"ok":true,"op":"skills","readable":{},"capsActive":{},"overrideCaps":{},"perkPoints":{},"perkTable":"{}",{},)"
					R"("characterLevel":{},"characterXp":{:.1f},"characterThreshold":{:.1f},"skills":[{}]}})",
					sk.readable ? "true" : "false",
					Patches::IsInstalled("Skill caps") ? "true" : "false",
					settings::skills::overrideCaps ? "true" : "false",
					perkPoints, EscapeJson(settings::levelup::PerksTableText()), attrs,
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
			// The Experience tab: op=experience reads it (controls, character xp/threshold, tallies, the last grant);
			// op=experience:<0|1> and op=skillspay:<0|1> flip the controls; "xpsim":"quest|location|cleared|kill|book"
			// grants that source's configured amount as if its event had fired (testing the grant path headlessly;
			// the kill uses the base amount plus one level).
			if (args.find("experience:") != std::string_view::npos)
			{
				settings::experience::enabled = ReadNumber(args, "experience:") != 0.0F;
				a_write(a_sink, std::format(R"({{"ok":true,"op":"experience","enabled":{}}})", settings::experience::enabled ? "true" : "false").c_str());
				return;
			}
			if (args.find("skillspay:") != std::string_view::npos)
			{
				settings::experience::skillsPay = ReadNumber(args, "skillspay:") != 0.0F;
				a_write(a_sink, std::format(R"({{"ok":true,"op":"skillspay","skillsPay":{}}})", settings::experience::skillsPay ? "true" : "false").c_str());
				return;
			}
			if (const auto sim = ExtractString(args, "xpsim"); !sim.empty())
			{
				using S = ExperienceSources::Source;
				S source = S::kQuest; float amount = settings::experience::questSide; std::string what = "simulated side quest";
				if (sim == "location") { source = S::kLocation; amount = settings::experience::location; what = "simulated location discovered"; }
				else if (sim == "cleared") { source = S::kCleared; amount = settings::experience::cleared; what = "simulated location cleared"; }
				else if (sim == "kill") { source = S::kKill; amount = settings::experience::killBase + settings::experience::killPerLevel; what = "simulated kill (level 1)"; }
				else if (sim == "book") { source = S::kBook; amount = settings::experience::book; what = "simulated book read"; }
				else if (sim != "quest") { a_write(a_sink, R"({"ok":false,"op":"xpsim","error":"quest|location|cleared|kill|book"})"); return; }
				if (!ExperienceSources::Controlling()) { a_write(a_sink, R"({"ok":false,"op":"xpsim","error":"the Experience tab is off or standing down"})"); return; }
				if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([source, amount, what]() { ExperienceSources::Grant(source, amount, what); }); }
				a_write(a_sink, std::format(R"({{"ok":true,"op":"xpsim","source":"{}","amount":{:.1f},"queued":true}})", sim, amount).c_str());
				return;
			}
			if (args.find("\"experience\"") != std::string_view::npos)
			{
				a_write(a_sink, std::format(R"({{"ok":true,"op":"experience","state":{}}})", ExperienceSources::StatusJson()).c_str());
				return;
			}
			// The Difficulty tab: op=difficultyvalues reads it (controls, the game's difficulty, every configured, LOADED
			// and LIVE value, the level table, the overhaul detection); op=damage:<0|1> / op=regen:<0|1> flip the
			// controls; op=dmgto<d>:<v> / op=dmgby<d>:<v> set a damage multiplier for difficulty d (0 Novice ..
			// 5 Legendary); op=regenhp<d>:<v> sets that difficulty's combat health regen rate. 1.1.2: op=shared:<0|1>,
			// op=sharedto:<v> / op=sharedby:<v> (one pair for every difficulty); op=bylevel:<0|1> and
			// op=levelfor<d>:<n> (the level table); op=checklevel runs the level rule now; preset:"loaded|vanilla|bb|requiem"
			// fills the table. Each applies at once (queued on the game thread).
			if (args.find("damage:") != std::string_view::npos)
			{
				settings::damage::control = ReadNumber(args, "damage:") != 0.0F;
				DifficultyValues::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"damage","control":{},"queued":true}})", settings::damage::control ? "true" : "false").c_str());
				return;
			}
			if (args.find("shared:") != std::string_view::npos)
			{
				settings::damage::sharedPair = ReadNumber(args, "shared:") != 0.0F;
				DifficultyValues::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"shared","sharedPair":{},"queued":true}})", settings::damage::sharedPair ? "true" : "false").c_str());
				return;
			}
			if (args.find("sharedto:") != std::string_view::npos)
			{
				settings::damage::sharedToPlayer = ReadNumber(args, "sharedto:");
				DifficultyValues::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"sharedto","value":{:.3f},"queued":true}})", settings::damage::sharedToPlayer).c_str());
				return;
			}
			if (args.find("sharedby:") != std::string_view::npos)
			{
				settings::damage::sharedByPlayer = ReadNumber(args, "sharedby:");
				DifficultyValues::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"sharedby","value":{:.3f},"queued":true}})", settings::damage::sharedByPlayer).c_str());
				return;
			}
			if (args.find("bylevel:") != std::string_view::npos)
			{
				settings::bylevel::enabled = ReadNumber(args, "bylevel:") != 0.0F;
				const int settled = DifficultyValues::ApplyLevelRule("DevBench bylevel");
				a_write(a_sink, std::format(R"({{"ok":true,"op":"bylevel","enabled":{},"settled":{}}})", settings::bylevel::enabled ? "true" : "false", settled).c_str());
				return;
			}
			if (args.find("\"checklevel\"") != std::string_view::npos)
			{
				const int settled = DifficultyValues::ApplyLevelRule("DevBench checklevel");
				a_write(a_sink, std::format(R"({{"ok":true,"op":"checklevel","settled":{},"state":{}}})", settled, DifficultyValues::StatusJson()).c_str());
				return;
			}
			for (int d = 0; d < 6; ++d)
			{
				const std::string lf = std::format("levelfor{}:", d);
				if (args.find(lf) != std::string_view::npos)
				{
					settings::bylevel::levelFor[d] = static_cast<std::uint32_t>(std::max(0.0F, ReadNumber(args, lf)));
					a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{}}})", lf, settings::bylevel::levelFor[d]).c_str());
					return;
				}
			}
			if (const auto at = args.find("\"preset\":\""); at != std::string_view::npos)
			{
				const auto start = at + 10;
				const auto end = args.find('"', start);
				const std::string which(args.substr(start, end == std::string_view::npos ? 0 : end - start));
				bool ok = true;
				if (which == "loaded") { DifficultyValues::UseLoadedValues(); }
				else if (which == "vanilla") { DifficultyValues::UseVanillaValues(); }
				else if (which == "bb") { DifficultyValues::UseBladeAndBlunt(); }
				else if (which == "requiem") { DifficultyValues::UseRequiem(); }
				else { ok = false; }
				if (ok) { DifficultyValues::RequestApply(); }
				a_write(a_sink, std::format(R"({{"ok":{},"op":"preset","preset":"{}","queued":{}}})", ok ? "true" : "false", which, ok ? "true" : "false").c_str());
				return;
			}
			if (args.find("regen:") != std::string_view::npos)
			{
				settings::regen::control = ReadNumber(args, "regen:") != 0.0F;
				DifficultyValues::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"regen","control":{},"queued":true}})", settings::regen::control ? "true" : "false").c_str());
				return;
			}
			for (int d = 0; d < 6; ++d)
			{
				const std::string to = std::format("dmgto{}:", d), by = std::format("dmgby{}:", d), hp = std::format("regenhp{}:", d);
				if (args.find(to) != std::string_view::npos) { settings::damage::toPlayer[d] = ReadNumber(args, to); DifficultyValues::RequestApply(); a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.3f},"queued":true}})", to, settings::damage::toPlayer[d]).c_str()); return; }
				if (args.find(by) != std::string_view::npos) { settings::damage::byPlayer[d] = ReadNumber(args, by); DifficultyValues::RequestApply(); a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.3f},"queued":true}})", by, settings::damage::byPlayer[d]).c_str()); return; }
				if (args.find(hp) != std::string_view::npos) { settings::regen::perDifficulty[0][d] = ReadNumber(args, hp); DifficultyValues::RequestApply(); a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.3f},"queued":true}})", hp, settings::regen::perDifficulty[0][d]).c_str()); return; }
			}
			if (args.find("\"difficultyvalues\"") != std::string_view::npos)
			{
				a_write(a_sink, std::format(R"({{"ok":true,"op":"difficultyvalues","state":{}}})", DifficultyValues::StatusJson()).c_str());
				return;
			}
			// op=difficulty reads the game's difficulty, the follow flag and the preset it maps to;
			// op=difficulty:<0..5> sets the game's difficulty as the Settings menu would, then syncs;
			// op=follow:<0|1> flips the follow flag. The driving surface for the per-difficulty feature.
			if (args.find("difficulty:") != std::string_view::npos)
			{
				const auto at = args.find("difficulty:") + 11;
				int wanted = -1;
				try { wanted = std::stoi(std::string(args.substr(at, 2))); } catch (...) { wanted = -1; }
				const bool ok = Difficulty::SetGameDifficulty(wanted);
				a_write(a_sink, std::format(R"({{"ok":{},"op":"difficulty","state":{}}})", ok ? "true" : "false", Difficulty::StatusJson()).c_str());
				return;
			}
			if (args.find("follow:") != std::string_view::npos)
			{
				const auto at = args.find("follow:") + 7;
				settings::difficulty::follow = (at < args.size() && args[at] == '1');
				const auto status = Difficulty::OnFollowChanged();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"follow","status":"{}","state":{}}})", EscapeJson(status), Difficulty::StatusJson()).c_str());
				return;
			}
			if (args.find("\"difficulty\"") != std::string_view::npos)
			{
				a_write(a_sink, std::format(R"({{"ok":true,"op":"difficulty","state":{}}})", Difficulty::StatusJson()).c_str());
				return;
			}
			// The Attributes tab: op=attributes reads it (control, count since which level, per-attribute rows);
			// op=attributes:<0|1> flips the starting-value control; op=starthealth:<n> / startmagicka:<n> /
			// startstamina:<n> set a starting value. Each applies at once (queued on the game thread).
			if (args.find("attributes:") != std::string_view::npos)
			{
				settings::attributes::control = ReadNumber(args, "attributes:") != 0.0F;
				Attributes::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"attributes","control":{},"queued":true}})", settings::attributes::control ? "true" : "false").c_str());
				return;
			}
			for (const auto& [key, index] : { std::pair{ "starthealth:", 0 }, std::pair{ "startmagicka:", 1 }, std::pair{ "startstamina:", 2 } })
			{
				if (args.find(key) != std::string_view::npos)
				{
					settings::attributes::starting[index] = ReadNumber(args, key);
					Attributes::RequestApply();
					a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.1f},"queued":true}})", key, settings::attributes::starting[index]).c_str());
					return;
				}
			}
			if (args.find("\"attributes\"") != std::string_view::npos)
			{
				a_write(a_sink, std::format(R"({{"ok":true,"op":"attributes","state":{}}})", Attributes::StatusJson()).c_str());
				return;
			}
			// The Carry Weight tab: op=carryweight reads it; op=carryweight:<0|1> flips the control; op=cwstart:<n>
			// and op=cwperlevel:<n> set the formula; op=cwapply recalculates now. Each applies at once.
			if (args.find("carryweight:") != std::string_view::npos)
			{
				settings::carryweight::control = ReadNumber(args, "carryweight:") != 0.0F;
				CarryWeight::RequestApply();
				a_write(a_sink, std::format(R"({{"ok":true,"op":"carryweight","control":{},"queued":true}})", settings::carryweight::control ? "true" : "false").c_str());
				return;
			}
			for (const auto& [key, target] : { std::pair{ "cwstart:", &settings::carryweight::starting }, std::pair{ "cwperlevel:", &settings::carryweight::perLevel } })
			{
				if (args.find(key) != std::string_view::npos)
				{
					*target = ReadNumber(args, key);
					CarryWeight::RequestApply();
					a_write(a_sink, std::format(R"({{"ok":true,"op":"{}","value":{:.1f},"queued":true}})", key, *target).c_str());
					return;
				}
			}
			if (args.find("\"cwapply\"") != std::string_view::npos)
			{
				CarryWeight::RequestApply();
				a_write(a_sink, R"({"ok":true,"op":"cwapply","queued":true})");
				return;
			}
			if (args.find("\"carryweight\"") != std::string_view::npos)
			{
				a_write(a_sink, std::format(R"({{"ok":true,"op":"carryweight","state":{}}})", CarryWeight::StatusJson()).c_str());
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
