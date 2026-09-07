# -*- coding: utf-8 -*-
"""lang-patch.py - one-shot, re-runnable language-support patch for Character Progression Control.

Applies the consumer-side mechanism from the translation rollout plan, section 2:

  * include/utils/Strings.h is vendored separately (copied from the template, unchanged);
  * include/SKSEMenuFramework.h gets "!ApocryphaMenuFramework" as the FIRST module lookup;
  * source/main.cpp calls strings::Configure("CharacterProgressionControl") first at kDataLoaded;
  * source/UI.cpp calls strings::Tick() as the first line of every page render callback and routes
    every drawn literal through strings::TR("CPC_...", "English");
  * source/DevBenchTool.cpp gains an op=strings on cpc.control returning strings::StatusJson().

Every edit is a must-match anchor replace: an anchor that is not found the expected number of times
raises, so a stale run against changed source fails loudly instead of leaving the code half
patched. Nothing is written until every anchor for that file has matched. Each file's patch is
skipped when its done-marker is already present, so the script is re-runnable.

Run: `python tools/lang-patch.py`, or `python tools/lang-patch.py --check` to verify every anchor
still matches without writing anything (used while the patch was built up page by page).

Encoding note: source files are read and written as UTF-8 with newline='' in BOTH directions, so a
raw CR inside a string literal survives and the existing line endings are preserved exactly (logic
library, 2026-09-02 - "Reading source with Python's universal newlines corrupts string literals").
This repo's own sources are LF; the vendored SKSEMenuFramework.h is CRLF, which is what fit() and
the COUNTED majority test in dominant_crlf() below are for.

NEVER routed through TR, deliberately:
  * SetSection("Character Progression Control") and the twelve AddSectionItem page names - they are
    the framework's registry identity (plan 2.2);
  * INI keys and the INI path, the Presets folder path, preset FILE names (including the default
    "My preset" typed into the name box) and the preset a difficulty maps to - data, not text;
  * game setting names (fEnchantingCostExponent and the rest, DifficultyValues::RegenSettingName /
    GlobalSettingName) - they are the game's own identifiers, printed so a player can find them;
  * patch-group names passed to Patches::IsInstalled / drawn from Patches::All(), and the
    compatibility entries from Compat::All() - those strings live in Patches.cpp and Compat.cpp,
    outside this page's source, and are left English this pass;
  * status text produced by other translation units (SkillPoints::MenuStatus(),
    CarryWeight::GetState().standDown, Difficulty::OnFollowChanged(),
    DifficultyValues::OverhaulLoaded()) - same reason;
  * product names used as button labels ("Vanilla", "Blade and Blunt", "Requiem");
  * slider display formats ("%.0f", "%.2f", ...), the "(?)" help marker and the "<-->" nudge
    ornament, the "    %s" indent format, and every logger:: line;
  * the "s" plural suffix in the Attributes readout - an English-only plural marker; the ten
    translations keep its %s at the end of the sentence where it reads as nothing.
"""
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECK_ONLY = "--check" in sys.argv


def read(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text):
    if CHECK_ONLY:
        return
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def fit(s, crlf):
    """Anchors are written with LF; a file kept in CRLF gets the CRLF form of the same text."""
    return s.replace("\n", "\r\n") if crlf else s


def dominant_crlf(text):
    """True when the file is KEPT in CRLF. COUNTED, not merely detected: an LF file can carry a
    handful of stray CRLF lines, and a plain `"\\r\\n" in text` would call the whole file CRLF and
    make every multi-line anchor miss."""
    return text.count("\r\n") * 2 > text.count("\n")


def apply_one(text, anchor, replacement, label, crlf=False, n=1):
    anchor, replacement = fit(anchor, crlf), fit(replacement, crlf)
    found = text.count(anchor)
    if found != n:
        raise RuntimeError("[{}] anchor found {} time(s), expected {}:\n{!r}".format(label, found, n, anchor))
    return text.replace(anchor, replacement, n)


def apply_all(text, pairs, label):
    crlf = dominant_crlf(text)
    for i, pair in enumerate(pairs):
        anchor, replacement = pair[0], pair[1]
        n = pair[2] if len(pair) > 2 else 1
        text = apply_one(text, anchor, replacement, "{}[{}]".format(label, i), crlf, n)
    return text


# ------------------------------------------------------------------------------------------------
# Pair builders. Every one of them leaves the key visible at the call site in the exact
# TR("KEY", "English") shape the generator and the check script grep for.
# ------------------------------------------------------------------------------------------------
def TR(key, text):
    return 'strings::TR("{}", "{}")'.format(key, text)


def sub(pairs, anchor, replacement, n=1):
    pairs.append((anchor, replacement, n))


def plain(pairs, key, call, text, n=1):
    """A Text/TextWrapped/TextDisabled/BulletText call whose whole argument is a plain literal."""
    sub(pairs, 'ImGuiMCP::{}("{}");'.format(call, text),
        'ImGuiMCP::{}("%s", {});'.format(call, TR(key, text)), n)


def fmt(pairs, key, call, text, n=1):
    """A literal that IS a printf format: the specifiers stay inside the translated text."""
    sub(pairs, 'ImGuiMCP::{}("{}",'.format(call, text),
        'ImGuiMCP::{}({},'.format(call, TR(key, text)), n)


def wrap(pairs, key, call, first, last, n=1):
    """A literal wrapped over several source lines: only the first and last fragments are anchored,
    so the file's own continuation indentation never has to be reproduced."""
    sub(pairs, 'ImGuiMCP::{}("{}"'.format(call, first),
        'ImGuiMCP::{}("%s", strings::TR("{}", "{}"'.format(call, key, first), n)
    sub(pairs, '"{}");'.format(last), '"{}"));'.format(last), n)


def wrapfmt(pairs, key, call, first, last, n=1):
    """The same, for a wrapped literal that is a printf format (arguments follow the last line)."""
    sub(pairs, 'ImGuiMCP::{}("{}"'.format(call, first),
        'ImGuiMCP::{}(strings::TR("{}", "{}"'.format(call, key, first), n)
    sub(pairs, '"{}",'.format(last), '"{}"),'.format(last), n)


def help1(pairs, key, text, n=1):
    sub(pairs, 'HelpMarker("{}");'.format(text), 'HelpMarker({});'.format(TR(key, text)), n)


def helpn(pairs, key, first, last, n=1):
    sub(pairs, 'HelpMarker("{}"'.format(first), 'HelpMarker(strings::TR("{}", "{}"'.format(key, first), n)
    sub(pairs, '"{}");'.format(last), '"{}"));'.format(last), n)


def toggle(pairs, key, label, var, n=1):
    sub(pairs, 'ImGuiMCP::Toggle("{}", &{})'.format(label, var),
        'ImGuiMCP::Toggle({}, &{})'.format(TR(key, label), var), n)


def slider(pairs, key, label, tail, n=1):
    sub(pairs, 'NudgeableSlider("{}", &{}'.format(label, tail),
        'NudgeableSlider({}, &{}'.format(TR(key, label), tail), n)


def button(pairs, key, label, n=1):
    sub(pairs, 'Button("{}")'.format(label), 'Button({})'.format(TR(key, label)), n)


def sep(pairs, key, label, n=1):
    sub(pairs, 'SeparatorText("{}")'.format(label), 'SeparatorText({})'.format(TR(key, label)), n)


def status(pairs, key, text, n=1):
    sub(pairs, 'statusMessage = "{}";'.format(text),
        'statusMessage = {};'.format(TR(key, text)), n)


def tick(pairs, panel):
    sub(pairs, 'void __stdcall {}::Render()\n\t{{\n'.format(panel),
        'void __stdcall {}::Render()\n\t{{\n\t\tstrings::Tick();\n\n'.format(panel))


# ------------------------------------------------------------------------------------------------
# 1) include/SKSEMenuFramework.h - "!ApocryphaMenuFramework" first, ahead of the alias name.
# ------------------------------------------------------------------------------------------------
def patch_skse_menu_framework_h():
    path = os.path.join(REPO, "include", "SKSEMenuFramework.h")
    text = read(path)
    if 'GetModuleHandleW(L"!ApocryphaMenuFramework")' in text:
        print("  SKSEMenuFramework.h: already patched")
        return
    anchor = (
        '        menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    replacement = (
        '        menuFramework = GetModuleHandleW(L"!ApocryphaMenuFramework");\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"ApocryphaMenuFramework");\n'
        '        }\n'
        '        if (!menuFramework) {\n'
        '            menuFramework = GetModuleHandleW(L"SKSEMenuFramework");\n'
        '        }\n'
    )
    text = apply_one(text, anchor, replacement, "SKSEMenuFramework.h:GetMenuFrameworkModule", dominant_crlf(text))
    write(path, text)
    print("  SKSEMenuFramework.h: patched")


# ------------------------------------------------------------------------------------------------
# 2) source/main.cpp - strings::Configure(...) FIRST at kDataLoaded, before anything is drawn or
#    the level-up menu's own strings are installed.
# ------------------------------------------------------------------------------------------------
MAIN_PAIRS = [
    ('#include "utils/Logger.h"\n',
     '#include "utils/Logger.h"\n#include "utils/Strings.h"\n'),
    ('\t\tcase SKSE::MessagingInterface::kDataLoaded:\n'
     '\t\t\t// Read the game\'s own values BEFORE anything is written, so "vanilla" means what\n',
     '\t\tcase SKSE::MessagingInterface::kDataLoaded:\n'
     '\t\t\t// Language first (translation rollout plan, section 2.1): the settings pages read\n'
     '\t\t\t// their text from Data/Interface/Translations/CharacterProgressionControl_<language>.txt\n'
     '\t\t\t// for whatever language the Apocrypha Menu Framework reports, before anything is drawn.\n'
     '\t\t\t// MenuStrings::Install() below reads the SAME file for the level-up menu SWF, but in\n'
     '\t\t\t// the GAME\'s language - that menu is drawn by the game, not by the framework.\n'
     '\t\t\tstrings::Configure("CharacterProgressionControl");\n'
     '\t\t\t// Read the game\'s own values BEFORE anything is written, so "vanilla" means what\n'),
]


def patch_main_cpp():
    path = os.path.join(REPO, "source", "main.cpp")
    text = read(path)
    if 'strings::Configure("CharacterProgressionControl")' in text:
        print("  main.cpp: already patched")
        return
    text = apply_all(text, MAIN_PAIRS, "main.cpp")
    write(path, text)
    print("  main.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 3) source/DevBenchTool.cpp - op=strings on cpc.control, and its descriptor line.
#
# Answered before every other op: the language the pages draw in is knowable whether or not a
# character is loaded, and the proof run reads it at the main menu.
# ------------------------------------------------------------------------------------------------
DEVBENCH_PAIRS = [
    ('#include "utils/Logger.h"\n',
     '#include "utils/Logger.h"\n#include "utils/Strings.h"\n'),
    ('\t\t\tconst std::string_view args = a_argsJson ? a_argsJson : "";\n',
     '\t\t\tconst std::string_view args = a_argsJson ? a_argsJson : "";\n'
     '\n'
     '\t\t\t// op=strings: which language the settings pages are drawing in, where that came from\n'
     '\t\t\t// and how many texts were read - the proof a translation file actually loaded,\n'
     '\t\t\t// readable without a capture and without a character.\n'
     '\t\t\tif (args.find("\\"strings\\"") != std::string_view::npos)\n'
     '\t\t\t{\n'
     '\t\t\t\ta_write(a_sink, (R"({"ok":true,"op":"strings","strings":)" + strings::StatusJson() + "}").c_str());\n'
     '\t\t\t\treturn;\n'
     '\t\t\t}\n'),
    ('"op=preset:<name> selects one; op=patches lists each engine patch group and whether it "\n',
     '"op=preset:<name> selects one; op=patches lists each engine patch group and whether it "\n'
     '\t\t\t"installed; op=strings reports the language the settings pages are drawn in, where "\n'
     '\t\t\t"that language came from and how many translated texts were loaded. "\n'),
]


def patch_devbench_tool_cpp():
    path = os.path.join(REPO, "source", "DevBenchTool.cpp")
    text = read(path)
    if '"op":"strings"' in text:
        print("  DevBenchTool.cpp: already patched")
        return
    text = apply_all(text, DEVBENCH_PAIRS, "DevBenchTool.cpp")
    write(path, text)
    print("  DevBenchTool.cpp: patched")


# ------------------------------------------------------------------------------------------------
# 4) source/UI.cpp - Tick() in all twelve page callbacks, TR() on every drawn literal.
# ------------------------------------------------------------------------------------------------
HELPERS = '''\t\t// Option lists and label tables are parallel key/label arrays: the key array carries the
\t\t// translation key, the label array the compiled English fallback. ComboTR() pairs them by
\t\t// index and rebuilds a combo's option list from TR'd entries every frame; a table's rows are
\t\t// translated at the use site (translation rollout plan, section 2.2). Only drawn text is in
\t\t// these arrays - never an INI key, a game setting name or a preset file name.
\t\tconstexpr const char* const kLogLevelKeys[] = { "CPC_Log_Trace", "CPC_Log_Debug", "CPC_Log_Info", "CPC_Log_Warning", "CPC_Log_Error", "CPC_Log_Critical", "CPC_Log_Off" };
\t\tconstexpr const char* const kLogLevelLabels[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
\t\tconstexpr int kLogLevelCount = 7;

\t\t// The eighteen skills in SkillList.h's order; the labels mirror skilllist::kDisplayName so the
\t\t// whole English list can be read out of this one file by the generator.
\t\tconstexpr const char* const kSkillKeys[] = {
\t\t\t"CPC_Skill_OneHanded", "CPC_Skill_TwoHanded", "CPC_Skill_Archery", "CPC_Skill_Block", "CPC_Skill_Smithing", "CPC_Skill_HeavyArmor",
\t\t\t"CPC_Skill_LightArmor", "CPC_Skill_Pickpocket", "CPC_Skill_Lockpicking", "CPC_Skill_Sneak", "CPC_Skill_Alchemy", "CPC_Skill_Speech",
\t\t\t"CPC_Skill_Alteration", "CPC_Skill_Conjuration", "CPC_Skill_Destruction", "CPC_Skill_Illusion", "CPC_Skill_Restoration", "CPC_Skill_Enchanting"
\t\t};
\t\tconstexpr const char* const kSkillLabels[] = {
\t\t\t"One-handed", "Two-handed", "Archery", "Block", "Smithing", "Heavy Armor",
\t\t\t"Light Armor", "Pickpocket", "Lockpicking", "Sneak", "Alchemy", "Speech",
\t\t\t"Alteration", "Conjuration", "Destruction", "Illusion", "Restoration", "Enchanting"
\t\t};
\t\tstatic_assert(sizeof(kSkillKeys) / sizeof(kSkillKeys[0]) == static_cast<std::size_t>(skilllist::kCount));

\t\tconstexpr const char* const kAttributeKeys[] = { "CPC_Attribute_Health", "CPC_Attribute_Magicka", "CPC_Attribute_Stamina" };
\t\tconstexpr const char* const kAttributeLabels[] = { "Health", "Magicka", "Stamina" };

\t\t// The six difficulties: the Difficulty tab names them and so does the Presets tab.
\t\tconstexpr const char* const kDifficultyKeys[] = { "CPC_Difficulty_Novice", "CPC_Difficulty_Apprentice", "CPC_Difficulty_Adept", "CPC_Difficulty_Expert", "CPC_Difficulty_Master", "CPC_Difficulty_Legendary" };
\t\tconstexpr const char* const kDifficultyLabels[] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };

\t\t// A Combo whose option list is rebuilt from TR'd entries every frame: store owns the text
\t\t// for the duration of the call, items is the array of pointers ImGui wants.
\t\tbool ComboTR(const char* a_label, int* a_current, const char* const* a_keys, const char* const* a_labels, int a_count)
\t\t{
\t\t\tstd::vector<std::string> store;
\t\t\tstore.reserve(static_cast<std::size_t>(a_count));
\t\t\tfor (int i = 0; i < a_count; ++i) { store.emplace_back(strings::TR(a_keys[i], a_labels[i])); }
\t\t\tstd::vector<const char*> items;
\t\t\titems.reserve(store.size());
\t\t\tfor (const auto& s : store) { items.push_back(s.c_str()); }
\t\t\treturn ImGuiMCP::Combo(a_label, a_current, items.data(), a_count);
\t\t}

\t\t// A translated printf format applied at runtime, for the few places that built their text
\t\t// with std::format before. The check script enforces that every translation of a key carries
\t\t// the same specifiers in the same order, so this is as safe as the compiled literal was.
\t\tstd::string TrFormat(const char* a_format, ...)
\t\t{
\t\t\tchar buf[1024];
\t\t\tva_list args;
\t\t\tva_start(args, a_format);
\t\t\tconst int written = std::vsnprintf(buf, sizeof(buf), a_format ? a_format : "", args);
\t\t\tva_end(args);
\t\t\treturn written > 0 ? std::string(buf) : std::string{};
\t\t}
'''


def ui_pairs():
    p = []

    # --- includes -------------------------------------------------------------------------------
    sub(p, '#include "utils/Logger.h"\n#include "utils/Toggle.h"',
        '#include "utils/Logger.h"\n#include "utils/Strings.h"\n#include "utils/Toggle.h"')
    sub(p, '#include <algorithm>\n#include <format>\n#include <functional>\n#include <string>\n',
        '#include <algorithm>\n#include <cstdarg>\n#include <cstdio>\n#include <format>\n'
        '#include <functional>\n#include <string>\n#include <vector>\n')

    # --- the key/label tables and the two helpers ------------------------------------------------
    sub(p, '\t\tconstexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };\n'
           '\t\tconstexpr int kLogLevelCount = 7;\n',
        HELPERS)

    # --- Tick() at the top of every page callback ------------------------------------------------
    for panel in ("LevellingPanel", "SkillsPanel", "LevelUpPanel", "AttributesPanel", "CarryWeightPanel",
                  "DifficultyPanel", "ExperiencePanel", "StaticLevellingPanel", "PresetsPanel",
                  "EnchantingPanel", "PatchesPanel", "DebugPanel"):
        tick(p, panel)

    # --- shared: the three buttons every tab carries, and the shared status lines -----------------
    button(p, "CPC_Btn_Save", "Save")
    status(p, "CPC_Status_Saving", "Saving...")
    sub(p, 'statusMessage = settings::Save() ? "Settings saved." : "Could not write the INI. See the log for why.";',
        'statusMessage = settings::Save() ? {} : {};'.format(
            TR("CPC_Status_Saved", "Settings saved."),
            TR("CPC_Status_SaveFailed", "Could not write the INI. See the log for why.")))
    help1(p, "CPC_Help_Save", "Writes every setting on these pages to the plugin's INI so it survives a restart.")
    button(p, "CPC_Btn_Reload", "Reload from INI")
    status(p, "CPC_Status_Reloading", "Reloading...")
    sub(p, 'statusMessage = ok ? "Settings reloaded from the INI."',
        'statusMessage = ok ? {}'.format(TR("CPC_Status_Reloaded", "Settings reloaded from the INI.")))
    sub(p, ': "Could not read the INI. See the log for why.";',
        ': {};'.format(TR("CPC_Status_ReloadFailed", "Could not read the INI. See the log for why.")))
    help1(p, "CPC_Help_Reload", "Throws away any change made here since the last save and re-reads the INI from disk.")
    button(p, "CPC_Btn_Defaults", "Restore defaults")
    status(p, "CPC_Status_DefaultsRestored", "Defaults restored - the values this install had before the mod. Press Save to keep them.")
    help1(p, "CPC_Help_Defaults", "Puts every setting back to the value this installation had before the mod touched it. Nothing is written until you press Save.")

    # Four tabs share these three: one key each, applied to every occurrence.
    sep(p, "CPC_Sec_RightNow", "What that means right now", 4)
    button(p, "CPC_Btn_ApplyNow", "Apply now", 4)
    status(p, "CPC_Status_Applied", "Applied.", 4)
    plain(p, "CPC_NoCharacter", "TextDisabled", "No character loaded yet.", 2)

    # --- InertNotice ------------------------------------------------------------------------------
    wrapfmt(p, "CPC_InertNotice", "TextWrapped",
            "Not active in this build: %s. The values below are remembered and ",
            "the Patches tab.")

    # --- Levelling --------------------------------------------------------------------------------
    wrap(p, "CPC_Lvl_Intro", "TextWrapped",
         "What one character level costs. Skyrim works out the experience needed for ",
         "your next level as:  base + (per-level x your level).")
    wrap(p, "CPC_Lvl_NotCaptured", "TextWrapped",
         "This install's own level-cost settings could not be read, so this tab will ",
         "not write anything. See the log.")
    sep(p, "CPC_Lvl_SecCost", "Cost of a level")
    toggle(p, "CPC_Lvl_Control", "Control the cost of a level", "over")
    helpn(p, "CPC_Lvl_HelpControl",
          "Off by default, and while it is off this mod writes nothing - your game levels exactly ",
          "two values below are applied and re-applied every time a save loads.")
    slider(p, "CPC_Lvl_Base", "Base cost", "levelling::base")
    help1(p, "CPC_Lvl_HelpBase", "The flat part of the cost, paid at every level. This install's own value is shown below.")
    slider(p, "CPC_Lvl_Mult", "Per level", "levelling::mult")
    helpn(p, "CPC_Lvl_HelpMult",
          "Added to the cost for each level you already have. Raising this makes later levels ",
          "progressively slower; lowering it flattens the curve.")
    fmt(p, "CPC_Lvl_CostLine", "Text", "Level %u to %u costs %.0f experience  (%.0f + %.1f x %u)")
    fmt(p, "CPC_Lvl_VanillaLine", "Text", "This install's own values: %.0f base, %.1f per level")
    plain(p, "CPC_Lvl_NotControlling", "TextDisabled", "Not controlling the cost - the values above are whatever the game is using.")
    helpn(p, "CPC_Lvl_HelpApply",
          "Re-writes the values. It also runs by itself when a save loads, when a new game starts, ",
          "and when you change anything above - nothing runs in the background.")

    # --- Skills -----------------------------------------------------------------------------------
    wrap(p, "CPC_Skl_Intro", "TextWrapped",
         "Where each skill stops, and what the game's own formulas read for it - ",
         "so a skill can show 300 while combat maths still treats it as 100.")
    wrap(p, "CPC_Skl_CapsInert", "TextWrapped",
         "The skill cap patch is NOT active in this build, so every skill still ",
         "and saved, but they do nothing yet. See the Patches tab.")
    toggle(p, "CPC_Skl_ControlCaps", "Control skill caps", "over")
    helpn(p, "CPC_Skl_HelpCaps",
          "Off by default. While it is off this mod asserts nothing about your skills - ",
          "with it off, not one instruction in the game is modified.")
    toggle(p, "CPC_Skl_ControlRates", "Control skill experience rates", "rates")
    help1(p, "CPC_Skl_HelpRates", "Off by default. Turns on the per-skill experience multipliers below.")
    plain(p, "CPC_Skl_ToLevelUnavailable", "TextDisabled", "Skill increase -> level: not available")
    wrap(p, "CPC_Skl_ExperienceOwns", "TextWrapped",
         "Experience is installed and owns where character experience ",
         "recommends them.")
    slider(p, "CPC_Skl_ToLevel", "Skill increase -> level", "skillexp::toLevelMult")
    helpn(p, "CPC_Skl_HelpToLevel",
          "Multiplies what a skill increase pays toward your CHARACTER level. This is ",
          "come faster without changing what a level costs.")
    sep(p, "CPC_Skl_SecNow", "Your skills right now")
    plain(p, "CPC_Skl_NoCharacter", "TextDisabled", "No character loaded - load a save to see your skills.")
    fmt(p, "CPC_Skl_CharacterLine", "Text", "Character level %u - %.0f of %.0f experience toward the next level")
    sub(p, 'ImGuiMCP::Text("%-12s  %5.1f   %.0f / %.0f", skilllist::kDisplayName[i],',
        'ImGuiMCP::Text({}, strings::TR(kSkillKeys[i], kSkillLabels[i]),'.format(
            TR("CPC_Skl_Row", "%-12s  %5.1f   %.0f / %.0f")))
    slider(p, "CPC_Skl_Cap", "Cap", "skills::cap[i]")
    help1(p, "CPC_Skl_HelpCap", "The level this skill stops advancing at. 100 is vanilla.")
    slider(p, "CPC_Skl_FormulaCap", "Formula cap", "skills::formulaCap[i]")
    helpn(p, "CPC_Skl_HelpFormulaCap",
          "The value the game's own calculations use for this skill, however high ",
          "balanced while the skill number keeps climbing.")
    slider(p, "CPC_Skl_Rate", "Experience rate", "skillexp::mult[i]")
    helpn(p, "CPC_Skl_HelpRate",
          "Multiplies what one use of this skill pays toward it. 1.00 is vanilla; ",
          "below 1 is slower, above 1 is faster.")

    # --- Level Up ---------------------------------------------------------------------------------
    wrap(p, "CPC_LvlUp_Intro", "TextWrapped",
         "What a level up gives you: the perk points, and the health, magicka, ",
         "stamina and carry weight that come with the choice you make.")
    sub(p, 'InertNotice("Attribute gains at level up", "the health/magicka/stamina and carry weight a level up grants are still vanilla; the perk table is separate");',
        'InertNotice("Attribute gains at level up", {});'.format(
            TR("CPC_LvlUp_Inert", "the health/magicka/stamina and carry weight a level up grants are still vanilla; the perk table is separate")))
    toggle(p, "CPC_LvlUp_Control", "Control what a level up grants", "over")
    help1(p, "CPC_LvlUp_HelpControl", "Off by default. While it is off this mod asserts nothing about level-up rewards.")
    sep(p, "CPC_LvlUp_SecPerks", "Perk points per level")
    wrap(p, "CPC_LvlUp_PerkIntro", "TextWrapped",
         "Whole perk points, as a table by level: from each listed level onward, that many per ",
         "level up. Vanilla is one row - from level 1, 1 perk.")
    sub(p, 'ImGuiMCP::InputInt("From level", &from)',
        'ImGuiMCP::InputInt({}, &from)'.format(TR("CPC_LvlUp_FromLevel", "From level")))
    sub(p, 'ImGuiMCP::InputInt("Perks", &perks)',
        'ImGuiMCP::InputInt({}, &perks)'.format(TR("CPC_LvlUp_Perks", "Perks")))
    sub(p, 'ImGuiMCP::SmallButton("Remove")',
        'ImGuiMCP::SmallButton({})'.format(TR("CPC_LvlUp_Remove", "Remove")))
    button(p, "CPC_LvlUp_AddRow", "Add a row")
    help1(p, "CPC_LvlUp_HelpRows", "The last row at or below the level reached applies. Each row is a whole number of perks.")
    plain(p, "CPC_LvlUp_SeeOtherTabs", "TextWrapped",
          "The health, magicka and stamina a level up grants are on the Attributes tab, and the carry weight per choice is on the Carry Weight tab; both apply while this is on.")

    # --- Attributes --------------------------------------------------------------------------------
    wrap(p, "CPC_Attr_Intro", "TextWrapped",
         "Starting health, magicka and stamina, and what each gains on a level up where you choose it. ",
         "it is installed on a character.")
    sub(p, 'InertNotice("Attribute gains at level up", "attribute choices are not being counted and a level up grants what vanilla grants");',
        'InertNotice("Attribute gains at level up", {});'.format(
            TR("CPC_Attr_Inert", "attribute choices are not being counted and a level up grants what vanilla grants")))
    toggle(p, "CPC_Attr_Control", "Control starting attributes", "control")
    helpn(p, "CPC_Attr_HelpControl",
          "Off by default. On: each starting value below is applied on top of your race's own start as this ",
          "mod's permanent modifier, and taken away again if you turn this off. Nothing is asserted while it is off.")
    sep(p, "CPC_Attr_SecStarting", "Starting values")
    slider(p, "CPC_Attr_StartHealth", "Starting health", "attributes::starting[0]")
    slider(p, "CPC_Attr_StartMagicka", "Starting magicka", "attributes::starting[1]")
    slider(p, "CPC_Attr_StartStamina", "Starting stamina", "attributes::starting[2]")
    helpn(p, "CPC_Attr_HelpStarting",
          "What the attribute starts at, for a character whose vanilla start is 100. A race that starts higher or ",
          "lower keeps its difference: the value is applied as (this - 100) on top of what the character started with.")
    sep(p, "CPC_Attr_SecGain", "Gain per level up")
    slider(p, "CPC_Attr_HealthPerLevel", "Health per level", "levelup::healthPerLevel")
    slider(p, "CPC_Attr_MagickaPerLevel", "Magicka per level", "levelup::magickaPerLevel")
    slider(p, "CPC_Attr_StaminaPerLevel", "Stamina per level", "levelup::staminaPerLevel")
    help1(p, "CPC_Attr_HelpGain", "What the chosen attribute gains. Vanilla is 10 for each.")
    plain(p, "CPC_Attr_NeedsLevelUpControl", "TextDisabled",
          'Applied while \\"Control what a level up grants\\" on the Level Up tab is on. It is off, so a level up grants vanilla\'s 10.')
    sub(p, '\t\t\tconstexpr const char* names[3] = { "Health", "Magicka", "Stamina" };\n',
        "\t\t\t// The three attribute names are kAttributeKeys/kAttributeLabels in the anonymous\n"
        "\t\t\t// namespace above, translated at the use site below.\n")
    # The count's noun becomes a WORD rather than an English "s" suffix: a plural marker glued onto
    # a translated sentence is wrong in most of the ten languages, and a printf argument may not
    # move position, so the marker cannot simply be pushed to the end. The drawn English is the same.
    sub(p, 'ImGuiMCP::Text("%s: %.0f base %+.0f from this mod %+.0f over %u investment%s = %.0f permanent   (now %.0f)",',
        '// The count\'s noun is a WORD, not an English "s" suffix: a plural marker glued onto\n'
        '\t\t\t\t// a translated sentence is wrong in most of the ten languages, and the drawn\n'
        '\t\t\t\t// English reads exactly as it did before.\n'
        '\t\t\t\tImGuiMCP::Text({},'.format(
            TR("CPC_Attr_Row", "%s: %.0f base %+.0f from this mod %+.0f over %u %s = %.0f permanent   (now %.0f)")))
    sub(p, 'names[i], r.base, r.applied, r.gained, r.invested, r.invested == 1 ? "" : "s", r.permanent, r.current);',
        'strings::TR(kAttributeKeys[i], kAttributeLabels[i]), r.base, r.applied, r.gained, r.invested,\n'
        '\t\t\t\t\t\t\t   r.invested == 1 ? {} : {},\n'
        '\t\t\t\t\t\t\t   r.permanent, r.current);'.format(
            TR("CPC_Attr_Investment", "investment"), TR("CPC_Attr_Investments", "investments")))
    wrapfmt(p, "CPC_Attr_SinceLevel", "TextWrapped",
            "Counted since level %u, when this mod first saw this character. The %u earlier level-ups are unknown ",
            "and are not guessed at: they are part of the base, with whatever else the character started with.")
    plain(p, "CPC_Attr_SinceLevelOne", "TextDisabled", "Counted since level 1 - the whole history is known.")
    plain(p, "CPC_Attr_NotCounting", "TextDisabled", "Not counting: the level-up patch is not attached (see the Patches tab).")
    helpn(p, "CPC_Attr_HelpApply",
          "Re-applies the starting values and refreshes the readout. It also runs by itself when a save loads, at each ",
          "level up, and when you change anything above - nothing runs in the background.")

    # --- Carry Weight ------------------------------------------------------------------------------
    wrap(p, "CPC_CW_Intro", "TextWrapped",
         "Carry weight as a formula of your level: starting + per level x (level - 1), applied to your permanent ",
         "level up, when you change a value here, and when you press Apply now - nothing runs in the background.")
    toggle(p, "CPC_CW_Control", "Control carry weight", "control")
    help1(p, "CPC_CW_HelpControl", "Off by default. While it is off nothing is asserted, and anything this mod had added is taken away again.")
    fmt(p, "CPC_CW_StandingDown", "TextWrapped", "Standing down: %s. Nothing here is applied while it is.")
    sep(p, "CPC_CW_SecFormula", "The formula")
    slider(p, "CPC_CW_Starting", "Starting carry weight", "carryweight::starting")
    help1(p, "CPC_CW_HelpStarting", "Carry weight at level 1. Vanilla is 300.")
    slider(p, "CPC_CW_PerLevel", "Per level", "carryweight::perLevel")
    help1(p, "CPC_CW_HelpPerLevel", "Added for every level after the first, whichever attribute you choose.")
    sep(p, "CPC_CW_SecPerChoice", "Per choice at level up")
    plain(p, "CPC_CW_OffWhileFormula", "TextDisabled",
          "Off while the formula controls carry weight: it sets the total, so a per-choice gain would be undone at the next recalculation.")
    slider(p, "CPC_CW_PerHealth", "...when health is chosen", "levelup::carryWeightPerHealth")
    slider(p, "CPC_CW_PerMagicka", "...when magicka is chosen", "levelup::carryWeightPerMagicka")
    slider(p, "CPC_CW_PerStamina", "...when stamina is chosen", "levelup::carryWeightPerStamina")
    help1(p, "CPC_CW_HelpCross", "The cross terms. Vanilla gives 5 carry weight with stamina and none with the other two.")
    plain(p, "CPC_CW_NeedsLevelUpControl", "TextDisabled",
          'Applied while \\"Control what a level up grants\\" on the Level Up tab is on. It is off, so stamina gives vanilla\'s 5.')
    plain(p, "CPC_CW_Zeroed", "TextDisabled", "Zeroed: one of our standalone carry-weight mods is loaded and owns carry weight.")
    fmt(p, "CPC_CW_FormulaLine", "Text", "Level %u: %.0f + %.1f x %u = %.0f")
    fmt(p, "CPC_CW_PermanentLine", "Text", "Permanent carry weight %.0f, now %.0f with temporary effects; net added by this mod %.0f")
    plain(p, "CPC_CW_NotControlling", "TextDisabled", "Not controlling carry weight - the values above are whatever the game is using.")
    help1(p, "CPC_CW_HelpApply", "Recalculates now. It also runs by itself when a save loads, at each level up, and when you change anything above.")

    # --- Difficulty ---------------------------------------------------------------------------------
    sub(p, '\t\tconstexpr const char* kNames[6] = { "Novice", "Apprentice", "Adept", "Expert", "Master", "Legendary" };\n',
        "\t\t// The six difficulty names are kDifficultyKeys/kDifficultyLabels in the anonymous\n"
        "\t\t// namespace above - the Presets tab names them too.\n")
    sub(p, '\t\tconstexpr const char* kRegenLabels[7] = { "Health regen rate in combat",',
        '\t\tconstexpr const char* const kRegenKeys[] = { "CPC_Regen_HealthRate", "CPC_Regen_MagickaRate", "CPC_Regen_StaminaRate",\n'
        '\t\t\t\t\t\t\t\t\t\t\t\t\t "CPC_Regen_HealthDelay", "CPC_Regen_MagickaDelay", "CPC_Regen_StaminaDelay",\n'
        '\t\t\t\t\t\t\t\t\t\t\t\t\t "CPC_Regen_DamagedDelay" };\n'
        '\t\tconstexpr const char* kRegenLabels[7] = { "Health regen rate in combat",')
    sub(p, '\t\tconstexpr const char* kGlobalLabels[5] = { "Health delay ceiling (s)",',
        '\t\tconstexpr const char* const kGlobalKeys[] = { "CPC_Global_HealthCeiling", "CPC_Global_MagickaCeiling", "CPC_Global_StaminaCeiling",\n'
        '\t\t\t\t\t\t\t\t\t\t\t\t\t  "CPC_Global_OutOfBreath", "CPC_Global_DownedEssential" };\n'
        '\t\tconstexpr const char* kGlobalLabels[5] = { "Health delay ceiling (s)",')
    wrap(p, "CPC_Diff_Intro", "TextWrapped",
         "Skyrim's own numbers per difficulty: the damage multipliers, and regeneration - which vanilla does not ",
         "vary by difficulty at all; this tab adds that. The difficulty you play on decides which set the game reads.")
    plain(p, "CPC_Diff_StandingDown", "TextWrapped",
          "Standing down: Custom Difficulty UI is loaded and owns these values. Nothing here is applied while it is.")
    sub(p, 'ImGuiMCP::Text("Game difficulty now: %s", active >= 0 ? kNames[active] : "none (no character loaded)");',
        'ImGuiMCP::Text({}, active >= 0 ? strings::TR(kDifficultyKeys[active], kDifficultyLabels[active]) : {});'.format(
            TR("CPC_Diff_Now", "Game difficulty now: %s"),
            TR("CPC_Diff_NoneLoaded", "none (no character loaded)")))
    wrapfmt(p, "CPC_Diff_OverhaulLoaded", "TextWrapped",
            "%s is loaded. Its damage multipliers and regeneration values are the loaded values shown below; ",
            "nothing here touches them until a control is switched on - then this tab writes last and supersedes them.")
    plain(p, "CPC_Diff_BBScaling", "TextWrapped",
          "BladeAndBlunt.ini has bLevelBasedDifficulty = true: its DLL steps the multipliers at levels 10 to 50 as well. Set it to false while this tab controls damage - two writers on one value is never stable. The Difficulty-by-level rule below does the same job.")
    plain(p, "CPC_Diff_BBNotFound", "TextWrapped",
          "BladeAndBlunt.ini was not found, so its bLevelBasedDifficulty could not be read. If it is true, set it to false while this tab controls damage.")
    sep(p, "CPC_Diff_SecDamage", "Damage")
    toggle(p, "CPC_Diff_ControlDamage", "Control damage multipliers", "dmg")
    helpn(p, "CPC_Diff_HelpDamage",
          "Off writes nothing: the multipliers your game loaded with (vanilla, or an overhaul's) stay exactly as they are, ",
          "difficulty you play on.")
    toggle(p, "CPC_Diff_SharedPair", "One pair for every difficulty", "shared")
    helpn(p, "CPC_Diff_HelpSharedPair",
          "On: the single pair below is written for all six difficulties, so the game's difficulty setting makes no ",
          "difference to damage. Off: each difficulty has its own pair.")
    slider(p, "CPC_Diff_DamageToYou", "Damage to you", "damage::sharedToPlayer")
    slider(p, "CPC_Diff_DamageByYou", "Damage by you", "damage::sharedByPlayer")
    help1(p, "CPC_Diff_HelpCtrlClick", "Ctrl+click a slider to type a value.")
    plain(p, "CPC_Diff_FillFrom", "Text", "Fill the table from:")
    button(p, "CPC_Diff_LoadedValues", "Loaded values")
    status(p, "CPC_Diff_StatusLoaded", "The table holds the values this game loaded with.")
    help1(p, "CPC_Diff_HelpLoadedValues", "Whatever your game holds at load - vanilla, or the overhaul you run. The starting point for tuning an overhaul without losing its numbers.")
    status(p, "CPC_Diff_StatusVanilla", "The table holds Skyrim's vanilla values.")
    status(p, "CPC_Diff_StatusBladeAndBlunt", "The table holds Blade and Blunt's values.")
    help1(p, "CPC_Diff_HelpBladeAndBlunt", "Its published pairs: to you as vanilla, by you 1.5 / 1.25 / 1 / 1 / 0.75 / 0.5.")
    status(p, "CPC_Diff_StatusRequiem", "The table holds Requiem's values.")
    help1(p, "CPC_Diff_HelpRequiem", "Every multiplier 1.0 - in Requiem the difficulty setting does no damage scaling by design.")
    sub(p, 'ImGuiMCP::SeparatorText(d == active ? std::format("{} (playing)", kNames[d]).c_str() : kNames[d]);',
        'const std::string difficultyName = strings::TR(kDifficultyKeys[d], kDifficultyLabels[d]);\n'
        '\t\t\t\t\tImGuiMCP::SeparatorText(d == active ? TrFormat({}, difficultyName.c_str()).c_str() : difficultyName.c_str());'.format(
            TR("CPC_Diff_Playing", "%s (playing)")))
    slider(p, "CPC_Diff_DamageToYou", "Damage to you", "damage::toPlayer[d]")
    slider(p, "CPC_Diff_DamageByYou", "Damage by you", "damage::byPlayer[d]")
    fmt(p, "CPC_Diff_LoadedWith", "TextDisabled", "    loaded with: x%.2f to you, x%.2f by you")
    help1(p, "CPC_Diff_HelpVanillaPairs", "Vanilla: to you 0.5 / 0.75 / 1 / 1.5 / 2 / 3, by you 2 / 1.5 / 1 / 0.75 / 0.5 / 0.25, Novice to Legendary. Ctrl+click a slider to type a value.")
    sep(p, "CPC_Diff_SecByLevel", "Difficulty by level")
    toggle(p, "CPC_Diff_ByLevel", "Set the game's difficulty from your level", "byLevel")
    helpn(p, "CPC_Diff_HelpByLevel",
          "On a save load and on every level-up, the highest difficulty whose level you have reached becomes the game's ",
          "0 = that difficulty is never chosen by this rule. Off: the game's difficulty is yours to set.")
    sub(p, 'ImGuiMCP::Text("Level %d -> %s", level, target >= 0 ? kNames[target] : "no row applies");',
        'ImGuiMCP::Text({}, level, target >= 0 ? strings::TR(kDifficultyKeys[target], kDifficultyLabels[target]) : {});'.format(
            TR("CPC_Diff_LevelMapsTo", "Level %d -> %s"),
            TR("CPC_Diff_NoRowApplies", "no row applies")))
    sub(p, 'ImGuiMCP::InputInt(std::format("{} from level", kNames[d]).c_str(), &from)',
        'ImGuiMCP::InputInt(TrFormat({}, strings::TR(kDifficultyKeys[d], kDifficultyLabels[d])).c_str(), &from)'.format(
            TR("CPC_Diff_FromLevel", "%s from level")))
    help1(p, "CPC_Diff_HelpMilestones", "Defaults are Blade and Blunt's milestones: one difficulty tier per ten levels.")
    sep(p, "CPC_Diff_SecRegen", "Regeneration")
    toggle(p, "CPC_Diff_ControlRegen", "Control regeneration", "rg")
    helpn(p, "CPC_Diff_HelpRegen",
          "Vanilla has one set of regeneration values for every difficulty. Here each difficulty has its own set, ",
          "difficulty in the game's Settings. Off restores the values your game came with.")
    sub(p, 'ImGuiMCP::Combo("Editing", &editing, kNames, 6);',
        'ComboTR({}, &editing, kDifficultyKeys, kDifficultyLabels, 6);'.format(TR("CPC_Diff_Editing", "Editing")))
    help1(p, "CPC_Diff_HelpEditing", "Which difficulty's set the sliders below show. The game reads the set for the difficulty you play on.")
    button(p, "CPC_Diff_CopyToAll", "Copy to all difficulties")
    help1(p, "CPC_Diff_HelpCopyToAll", "Every other difficulty's set becomes a copy of this one.")
    sub(p, 'ImGuiMCP::TextDisabled("%s - not in this game\'s settings", kRegenLabels[i]);',
        'ImGuiMCP::TextDisabled({}, strings::TR(kRegenKeys[i], kRegenLabels[i]));'.format(
            TR("CPC_Diff_NotInSettings", "%s - not in this game's settings")))
    sub(p, 'NudgeableSlider(kRegenLabels[i],', 'NudgeableSlider(strings::TR(kRegenKeys[i], kRegenLabels[i]),')
    sub(p, 'HelpMarker(std::format("{} - your game\'s own value is {:.2f}.", DifficultyValues::RegenSettingName(i), DifficultyValues::RegenVanilla(i)).c_str());',
        'HelpMarker(TrFormat({}, DifficultyValues::RegenSettingName(i), DifficultyValues::RegenVanilla(i)).c_str());'.format(
            TR("CPC_Diff_HelpRegenValue", "%s - your game's own value is %.2f.")))
    sep(p, "CPC_Diff_SecEveryDifficulty", "Every difficulty")
    sub(p, 'ImGuiMCP::TextDisabled("%s - not in this game\'s settings", kGlobalLabels[i]);',
        'ImGuiMCP::TextDisabled({}, strings::TR(kGlobalKeys[i], kGlobalLabels[i]));'.format(
            TR("CPC_Diff_NotInSettings", "%s - not in this game's settings")))
    sub(p, 'NudgeableSlider(kGlobalLabels[i],', 'NudgeableSlider(strings::TR(kGlobalKeys[i], kGlobalLabels[i]),')
    sub(p, 'HelpMarker(std::format("{} - one value for every difficulty; your game\'s own value is {:.2f}.", DifficultyValues::GlobalSettingName(i), DifficultyValues::GlobalVanilla(i)).c_str());',
        'HelpMarker(TrFormat({}, DifficultyValues::GlobalSettingName(i), DifficultyValues::GlobalVanilla(i)).c_str());'.format(
            TR("CPC_Diff_HelpGlobalValue", "%s - one value for every difficulty; your game's own value is %.2f.")))
    sep(p, "CPC_Diff_SecUsingNow", "What the game is using right now")
    sub(p, 'ImGuiMCP::Text("Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)", kNames[active],',
        'ImGuiMCP::Text({}, strings::TR(kDifficultyKeys[active], kDifficultyLabels[active]),'.format(
            TR("CPC_Diff_DamageLine", "Damage at %s: x%.2f to you, x%.2f by you (loaded with x%.2f / x%.2f)")))
    fmt(p, "CPC_Diff_RegenLine", "Text", "Regeneration in combat: health x%.2f, magicka x%.2f, stamina x%.2f")
    fmt(p, "CPC_Diff_DelayLine", "Text", "Delay after damage: health %.1f s, magicka %.1f s, stamina %.1f s")
    plain(p, "CPC_Diff_NotControlling", "TextDisabled", "Not controlling either - nothing is written; the values above are whatever the game loaded with.")
    help1(p, "CPC_Diff_HelpApply", "Re-writes the values. It also runs by itself when a save loads, when the game's difficulty changes, and when you change anything above.")

    # --- Experience -----------------------------------------------------------------------------------
    wrap(p, "CPC_Exp_Intro", "TextWrapped",
         "Character experience from what you do: quests completed, places discovered, dungeons cleared, kills ",
         "against the Levelling tab's level cost.")
    plain(p, "CPC_Exp_StandingDown", "TextWrapped",
          "Standing down: the Experience mod is loaded and owns where experience comes from. Nothing here is granted while it is.")
    toggle(p, "CPC_Exp_Enabled", "Earn experience from quests, exploration and kills", "on")
    help1(p, "CPC_Exp_HelpEnabled", "Off by default. While it is off nothing is granted and the game levels exactly as before.")
    toggle(p, "CPC_Exp_SkillsPay", "Skill increases still pay toward your level", "pay")
    helpn(p, "CPC_Exp_HelpSkillsPay",
          "On: the sources below add to what skill use already pays (supplement). Off: skill increases pay nothing ",
          "toward your level and the sources below are the only way to level (replace).")
    plain(p, "CPC_Exp_RestartNeeded", "TextWrapped",
          "Restart the game for skill increases to stop paying: the level-income patch attaches at startup and was not needed when this session started.")
    sep(p, "CPC_Exp_SecQuests", "Quests completed, by kind")
    slider(p, "CPC_Exp_QuestMain", "Main quest", "experience::questMain")
    slider(p, "CPC_Exp_QuestFaction", "Faction quest", "experience::questFaction")
    help1(p, "CPC_Exp_HelpFaction", "The guilds, the Companions, the Dark Brotherhood, the civil war, and the Dawnguard and Dragonborn lines.")
    slider(p, "CPC_Exp_QuestDaedric", "Daedric quest", "experience::questDaedric")
    slider(p, "CPC_Exp_QuestSide", "Side quest", "experience::questSide")
    slider(p, "CPC_Exp_QuestMisc", "Miscellaneous objective", "experience::questMisc")
    slider(p, "CPC_Exp_QuestOther", "Other quest", "experience::questOther")
    help1(p, "CPC_Exp_HelpOther", "Quests the game gives no kind - many mod-added ones.")
    sep(p, "CPC_Exp_SecExploration", "Exploration")
    slider(p, "CPC_Exp_Location", "Location discovered", "experience::location")
    slider(p, "CPC_Exp_Cleared", "Location cleared", "experience::cleared")
    sep(p, "CPC_Exp_SecKills", "Kills")
    slider(p, "CPC_Exp_KillBase", "Per kill", "experience::killBase")
    slider(p, "CPC_Exp_KillPerLevel", "Plus, per level of the victim", "experience::killPerLevel")
    toggle(p, "CPC_Exp_FollowerKills", "Count kills by followers and summons", "fk")
    sep(p, "CPC_Exp_SecBooks", "Books")
    slider(p, "CPC_Exp_Book", "Book read", "experience::book")
    slider(p, "CPC_Exp_SkillBook", "Skill book read", "experience::skillBook")
    fmt(p, "CPC_Exp_LevelLine", "Text", "Level %u: %.0f of %.0f experience toward the next")
    fmt(p, "CPC_Exp_CountsLine", "Text", "This character, since the count began: %u quests (%.0f), %u locations (%.0f), %u cleared (%.0f), %u kills (%.0f), %u books (%.0f)")
    fmt(p, "CPC_Exp_Last", "Text", "Last: %s")
    plain(p, "CPC_Exp_NotGranting", "TextDisabled", "Not granting anything - experience comes from skill use, as in vanilla.")

    # --- Static Levelling -------------------------------------------------------------------------------
    wrap(p, "CPC_Stat_Intro", "TextWrapped",
         "A fixed amount of experience per use of a skill, instead of vanilla's ",
         "scaling - so a skill advances at the same rate at level 5 and level 50.")
    sub(p, 'InertNotice("Static levelling", "skill experience still scales the vanilla way");',
        'InertNotice("Static levelling", {});'.format(
            TR("CPC_Stat_Inert", "skill experience still scales the vanilla way")))
    toggle(p, "CPC_Stat_Enabled", "Use static skill levelling", "on")
    help1(p, "CPC_Stat_HelpEnabled", "Off by default. While it is off, nothing about skill experience is asserted.")
    sep(p, "CPC_Stat_SecPerUse", "Experience per use, as a percent of a skill level")
    helpn(p, "CPC_Stat_HelpPerUse",
          "1.0 = one use is one percent of the level, so 100 uses raise the skill by one level whether it is at 5 or at 50. ",
          "Perk bonuses to skill use are not applied while this is on - the amount stays fixed.")
    sub(p, 'NudgeableSlider(skilllist::kDisplayName[i], &staticlevel::xpPerUse[i]',
        'NudgeableSlider(strings::TR(kSkillKeys[i], kSkillLabels[i]), &staticlevel::xpPerUse[i]')
    sep(p, "CPC_Stat_SecPoints", "Skill points")
    wrap(p, "CPC_Stat_PointsIntro", "TextWrapped",
         "Skills advance only by points spent in the level-up menu: each level grants points, the menu ",
         "skill-point level-up menu file (this mod ships one; Static Skill Leveling Rewritten's skins fit too).")
    toggle(p, "CPC_Stat_UsePoints", "Use skill points", "pts")
    helpn(p, "CPC_Stat_HelpUsePoints",
          "Off by default. While on, ordinary skill experience is not banked - use only counts through points - and a ",
          "point-spent level pays nothing toward the character level. Takes effect after a restart.")
    sub(p, 'ImGuiMCP::InputInt("Points per level", &staticlevel::pointsPerLevel)',
        'ImGuiMCP::InputInt({}, &staticlevel::pointsPerLevel)'.format(TR("CPC_Stat_PointsPerLevel", "Points per level")))
    slider(p, "CPC_Stat_PointsLevelMult", "Plus, per character level", "staticlevel::pointsLevelMult")
    help1(p, "CPC_Stat_HelpPointsMult", "Points granted at a level = points per level + this x the level, whole numbers. Negative slows the curve.")
    sub(p, 'ImGuiMCP::InputInt("Bank cap (0 = none)", &staticlevel::pointsCap)',
        'ImGuiMCP::InputInt({}, &staticlevel::pointsCap)'.format(TR("CPC_Stat_BankCap", "Bank cap (0 = none)")))
    sub(p, 'ImGuiMCP::InputInt("Most increases per skill, per level up", &staticlevel::maxIncreasesPerSkill)',
        'ImGuiMCP::InputInt({}, &staticlevel::maxIncreasesPerSkill)'.format(
            TR("CPC_Stat_MaxIncreases", "Most increases per skill, per level up")))
    plain(p, "CPC_Stat_CostIntro", "TextWrapped", "Cost of one skill level, by the skill's current level:")
    sub(p, '\t\t\tconst char* tiers[4] = { "Below 25", "25 to 49", "50 to 74", "75 and up" };\n',
        '\t\t\tconstexpr const char* const kTierKeys[] = { "CPC_Stat_TierBelow25", "CPC_Stat_Tier25", "CPC_Stat_Tier50", "CPC_Stat_Tier75" };\n'
        '\t\t\tconstexpr const char* const kTierLabels[] = { "Below 25", "25 to 49", "50 to 74", "75 and up" };\n')
    sub(p, 'ImGuiMCP::InputInt(tiers[i], &staticlevel::cost[i])',
        'ImGuiMCP::InputInt(strings::TR(kTierKeys[i], kTierLabels[i]), &staticlevel::cost[i])')
    fmt(p, "CPC_Stat_BankLine", "Text", "This character: %d point(s) banked; the next level grants %d.")

    # --- Presets ------------------------------------------------------------------------------------------
    wrap(p, "CPC_Pre_Intro", "TextWrapped",
         "A preset is a file in this mod's Presets folder, in the same format as its ",
         "on different presets at once.")
    fmt(p, "CPC_Pre_Using", "Text", "This character is using: %s")
    sep(p, "CPC_Pre_SecDifficulty", "Game difficulty")
    toggle(p, "CPC_Pre_Follow", "Follow the game's difficulty", "follow")
    helpn(p, "CPC_Pre_HelpFollow",
          "One configuration per difficulty. While this is on, the difficulty set in the game's own ",
          "Saved with the mod's INI, so it stays on between sessions.")
    sub(p, 'ImGuiMCP::Text("The game is on %s%s", Difficulty::CurrentName(),',
        'const int gameDifficulty = Difficulty::Current();\n'
        '\t\tconst std::string difficultyName = gameDifficulty >= 0\n'
        '\t\t\t\t\t\t\t\t\t\t\t ? strings::TR(kDifficultyKeys[gameDifficulty], kDifficultyLabels[gameDifficulty])\n'
        '\t\t\t\t\t\t\t\t\t\t\t : {};\n'
        '\t\t// The preset a difficulty maps to is a FILE name - never translated, only quoted.\n'
        '\t\tconst std::string presetSuffix = settings::difficulty::follow\n'
        '\t\t\t\t\t\t\t\t\t\t\t ? TrFormat({}, Difficulty::PresetNameFor(gameDifficulty).c_str())\n'
        '\t\t\t\t\t\t\t\t\t\t\t : std::string{{}};\n'
        '\t\tImGuiMCP::Text({}, difficultyName.c_str(),'.format(
            TR("CPC_Difficulty_NoneShort", "none"),
            TR("CPC_Pre_ArrowPreset", ' -> preset \\"%s\\"'),
            TR("CPC_Pre_GameIsOn", "The game is on %s%s")))
    sub(p, 'settings::difficulty::follow ? (" -> preset \\"" + Difficulty::PresetNameFor(Difficulty::Current()) + "\\"").c_str() : "");',
        'presetSuffix.c_str());')
    sep(p, "CPC_Pre_SecPresets", "Presets")
    sub(p, 'ImGuiMCP::Button(isCurrent ? "In use" : "Use")',
        'ImGuiMCP::Button(isCurrent ? {} : {})'.format(
            TR("CPC_Pre_InUse", "In use"), TR("CPC_Pre_Use", "Use")))
    sub(p, '? "Now using " + picked + "."',
        '? TrFormat({}, picked.c_str())'.format(TR("CPC_Pre_NowUsing", "Now using %s.")))
    sub(p, ': "Could not read that preset - fell back to the built-in default.";',
        ': std::string({});'.format(TR("CPC_Pre_SelectFailed", "Could not read that preset - fell back to the built-in default.")))
    sep(p, "CPC_Pre_SecThisConfig", "This configuration")
    button(p, "CPC_Pre_SaveInto", "Save into the selected preset")
    sub(p, '? "Saved into " + Presets::Current() + "."',
        '? TrFormat({}, Presets::Current().c_str())'.format(TR("CPC_Pre_SavedInto", "Saved into %s.")))
    sub(p, ': "The built-in default is not a file - use Save as a new preset instead.";',
        ': std::string({});'.format(TR("CPC_Pre_SaveIntoFailed", "The built-in default is not a file - use Save as a new preset instead.")))
    helpn(p, "CPC_Pre_HelpSaveInto",
          "A preset is a living configuration, not a read-only template: this writes what is on ",
          "the pages now back into the preset you are using.")
    sub(p, 'ImGuiMCP::InputText("New preset name", newName, sizeof(newName));',
        'ImGuiMCP::InputText({}, newName, sizeof(newName));'.format(TR("CPC_Pre_NewName", "New preset name")))
    button(p, "CPC_Pre_SaveAsNew", "Save as a new preset")
    sub(p, '? "Wrote and selected " + wanted + "."',
        '? TrFormat({}, wanted.c_str())'.format(TR("CPC_Pre_WroteAndSelected", "Wrote and selected %s.")))
    sub(p, ': "That name cannot be used as a file name.";',
        ': std::string({});'.format(TR("CPC_Pre_BadName", "That name cannot be used as a file name.")))
    help1(p, "CPC_Pre_HelpSaveAsNew", "Writes the current configuration out as its own file, which is also how you share one.")
    button(p, "CPC_Pre_Delete", "Delete the selected preset")
    sub(p, '? "Deleted " + doomed + "; back on the built-in default."',
        '? TrFormat({}, doomed.c_str())'.format(TR("CPC_Pre_Deleted", "Deleted %s; back on the built-in default.")))
    sub(p, ': "Could not delete that preset.";',
        ': std::string({});'.format(TR("CPC_Pre_DeleteFailed", "Could not delete that preset.")))
    helpn(p, "CPC_Pre_HelpDelete",
          "Deletes the file. The built-in default can never be deleted, so there is always ",
          "somewhere to fall back to.")

    # --- Enchanting ------------------------------------------------------------------------------------
    wrap(p, "CPC_Ench_Intro", "TextWrapped",
         "How the cost of using an enchanted item scales with your Enchanting skill. ",
         "sits beside the skill caps.")
    wrap(p, "CPC_Ench_NotCaptured", "TextWrapped",
         "None of the enchanting cost settings could be read on this runtime, so ",
         "this tab will not write anything. See the log.")
    plain(p, "CPC_Ench_Partial", "TextWrapped", "Only some of these settings exist on this runtime; the rest are left alone.")
    toggle(p, "CPC_Ench_Control", "Control the enchantment charge cost", "over")
    helpn(p, "CPC_Ench_HelpControl",
          "Off by default. While it is off this mod restores the values your install came with ",
          "and leaves them alone.")
    slider(p, "CPC_Ench_Base", "Cost base", "enchanting::costBase")
    slider(p, "CPC_Ench_Scale", "Cost scale", "enchanting::costScale")
    slider(p, "CPC_Ench_Mult", "Cost multiplier", "enchanting::costMult")
    slider(p, "CPC_Ench_Exponent", "Cost exponent", "enchanting::costExponent")
    helpn(p, "CPC_Ench_HelpValues",
          "Lower values make an enchanted item cheaper to use. The exponent is the one that ",
          "runs away when the skill climbs past 100.")
    sep(p, "CPC_Ench_SecUsing", "What the game is using")
    fmt(p, "CPC_Ench_NotOnRuntime", "TextDisabled", "%s - not on this runtime")
    fmt(p, "CPC_Ench_ValueLine", "Text", "%s = %.3f   (this install: %.3f)")

    # --- Patches ------------------------------------------------------------------------------------------
    wrap(p, "CPC_Pat_Intro", "TextWrapped",
         "Each engine patch this mod installs, and what it changes. A patch that is ",
         "would without this mod.")
    sep(p, "CPC_Pat_SecWhatElse", "What else is installed")
    wrap(p, "CPC_Pat_DetectedIntro", "TextWrapped",
         "Detected at load. A conflict found here switches OUR feature off - ",
         "never another mod's - and every switch stays yours to override.")
    fmt(p, "CPC_Pat_Installed", "Text", "%s: installed")
    fmt(p, "CPC_Pat_NotInstalled", "TextDisabled", "%s: not installed")
    sep(p, "CPC_Pat_SecEnginePatches", "Engine patches")
    plain(p, "CPC_Pat_NoGroups", "TextDisabled", "No patch groups are registered in this build.")
    sub(p, 'ImGuiMCP::Text("%s", g.installed ? "Active" : "Not active");',
        'ImGuiMCP::Text("%s", g.installed ? {} : {});'.format(
            TR("CPC_Pat_Active", "Active"), TR("CPC_Pat_NotActive", "Not active")))
    fmt(p, "CPC_Pat_Touches", "TextWrapped", "Touches: %s")
    fmt(p, "CPC_Pat_Why", "TextWrapped", "Why: %s")

    # --- Debug ---------------------------------------------------------------------------------------------
    sep(p, "CPC_Dbg_Sec", "Debug")
    sub(p, 'ImGuiMCP::Combo("Log level", &level, kLogLevelNames, kLogLevelCount)',
        'ComboTR({}, &level, kLogLevelKeys, kLogLevelLabels, kLogLevelCount)'.format(
            TR("CPC_Dbg_LogLevel", "Log level")))
    help1(p, "CPC_Dbg_HelpLogLevel",
          r"Applies immediately. The log is at Documents\\My Games\\Skyrim Special Edition\\SKSE\\CharacterProgressionControl.log.")
    fmt(p, "CPC_Dbg_Applications", "Text", "Level cost applied %llu time(s) this session")
    fmt(p, "CPC_Dbg_LiveSettings", "Text", "Live game settings: base %.1f, per level %.1f")

    return p


def patch_ui_cpp():
    path = os.path.join(REPO, "source", "UI.cpp")
    text = read(path)
    if "strings::Tick();" in text:
        print("  UI.cpp: already patched")
        return
    text = apply_all(text, ui_pairs(), "UI.cpp")
    write(path, text)
    print("  UI.cpp: patched")


def main():
    print("lang-patch{}: {}".format(" --check" if CHECK_ONLY else "", REPO))
    patch_skse_menu_framework_h()
    patch_main_cpp()
    patch_devbench_tool_cpp()
    patch_ui_cpp()
    print("done")


if __name__ == "__main__":
    main()
