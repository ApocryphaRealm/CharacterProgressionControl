# Character Progression Control

Version 1.0.7

What changed in 1.0.6: a respec of the skill points - any mod can send the SKSE mod event
CPC_RefundSkillPoints (Potion of Clarity does) and every skill above its starting value goes back to
it, its experience clears, and the points this mod's cost tiers charged for the levels above come
back to the bank; a point-bought skill level now pays nothing toward the character level.

One page for how a character grows: what a level costs, where experience comes from, skill caps
and rates, and what a level up grants. SKSE plugin, C++ with CommonLibSSE-NG, MIT, written from
scratch. Skyrim Special Edition.

**Version 1.0.4 - stages 1 to 8 of nine, every engine patch verified in game on Skyrim SE 1.5.97
(2026-09-05), and the skill-point half of static levelling added.** The plan is at `4. plans\character-progression-control`. Stage 9 (alternative
experience sources) is a separate decision and is not started.

What each patch group does, and how it was proven, is on the Patches tab in game and in the
source comments; the short version: the skill cap and formula cap (Skills tab), the skill
experience rates and skill-to-level income (Skills tab), the perk table and attribute gains with
their carry-weight cross terms (Level Up tab), and static levelling (its own tab) are all live.
Every site is found by its byte shape in the running game, matched exactly once, proven by what
it loads or compares before a byte is written, and with its setting off nothing is written at all.

**Two build lines (1.0.5).** The package is a FOMOD: one DLL for Skyrim SE 1.5.97 / AE 1.6.1170
(CommonLibSSE-NG 3.7.0, MIT throughout) and one for Skyrim 1.7.99 / 1.7.104 (CommonLibSSE-NG 7.2.0,
distributed under GPL-3.0-or-later with the library's exceptions - its licence texts install with
it). Every site is found by shape at runtime, so a group simply reports itself off on a build where
its shape is absent: on 1.7.104 the skill caps, formula caps and attribute gains attach; the skill
experience rates, skill-to-level income, static levelling and perk-table sites are not present there
and those groups stay off and say so on the Patches tab (skill points still work, but ordinary skill
experience cannot be suppressed on that build and the group says that too).

**Skill points (1.0.4).** With *Use skill points* on, skills advance only by points spent in the
level-up menu: each level grants `points per level + multiplier x level` points, banked when
unspent and kept in the co-save; the menu shows every skill with + and -, the four cost tiers
(below 25, 25-49, 50-74, 75 and up) and a cap on increases per skill per level up; the choice is
applied when the attribute is picked, through the game's own skill-improve path so the usual
"skill increased" notice fires. Ordinary skill experience is not banked while it is on, and a
point-spent level pays nothing toward the character level. The menu is Static Skill Leveling
Rewritten's vanilla-look `Interface\levelupmenu.swf`, shipped with its authors' permission (see
`dist\Interface\levelupmenu-CREDIT.txt`); its other skins fit the same contract. The DLL does what
that mod's Papyrus did: it feeds the movie the caps, the settings and the player's skills when the
menu opens and listens for the allocation the movie sends back.

Finding that address is worth explaining, because it governs all future hook work here. The
retail `SkyrimSE.exe` is **Steam-packed**: it carries a `.bind` section, its entry point sits
inside it, and the real `.text` is encrypted on disk. So a byte signature cannot be checked
against the shipped file at all - only against the decrypted image in the running process. The
mod scans for its signatures at load and refuses any that matches zero times *or* more than once,
and `cpc.control op=scan` exposes the same scanner for finding new ones.

## What is in this build

- **Levelling tab** - what a character level costs. Skyrim works the cost of your next level out
  as `base + (per-level x your level)`, and this tab owns those two numbers. It needs no engine
  patch of any kind, which is why it is first.
- **Skills tab** - every skill live (level, experience, threshold) straight from the game's own
  progression data, plus the cap and formula-cap settings for all eighteen. The cap patch itself
  is not implemented yet and the tab says so; the values are stored and saved regardless.
- **Enchanting tab** - how the cost of using an enchanted item scales with the Enchanting skill.
  This is the equation uncapping Enchanting breaks, which is why it sits beside the caps. Another
  no-hook tab: they are the game's own settings.
- **Presets tab** - a preset is a file in this mod's Presets folder, in the same format as its
  INI. Whichever is selected is what the mod is using - no hidden merge - and **the selection
  belongs to the character** while the files are shared, so two saves can sit on different presets
  at once. The default is compiled into the DLL, so it can never be deleted or corrupted and a
  missing preset falls back to it rather than to zeros.
- **Patches tab** - each engine patch group and whether it installed. A patch that is not active
  is not a fault: that part of the game behaves as it would without this mod, and it says which.
- **Debug tab** - log level, and a live readout of what the game is actually using.
- Plain-file INI, the tabbed settings pages under the menu framework, and the `cpc.control`
  DevBench tool so the whole thing can be driven and read without a person at the keyboard.

Still to come, and every one of them needs an engine hook: the skill cap patch itself, experience
rates and curves, perks per level, attributes, static levelling, and alternative experience
sources.

## Two rules this mod follows, and will keep following

**It writes nothing until you ask it to.** The level-cost override is off by default, and while
it is off the mod does not touch the game's settings - another mod that sets them keeps them.
Installing this must never silently change how somebody levels.

**"Default" means what your install had.** Before anything is written, the mod reads the game's
own level-cost values and remembers them. The page is seeded from those on first run, and
*Restore defaults* goes back to them - not to a number compiled into the DLL from some other copy
of the game.

## Settings

`Data\SKSE\Plugins\CharacterProgressionControl.ini`, read with our own parser and never through
the Windows profile API, so a profile redirector can neither serve stale values nor rewrite the
file. Every value is also editable in game.

## Build

- `configure.bat` once, then `build.bat`.
- The DLL and its `.pdb` are what ship; the `.pdb` goes in the main package so a crash log
  resolves our frames.

## Licence and credit

MIT, original code, no assets or code taken from any other mod. Credited as inspiration, with
nothing derived from them: the Skyrim Skill Uncapper lineage (Kassent, Vadfromnu, Elys, Kasplat),
Charmics for Leveling Freedom, the Static Skill Leveling Rewritten authors (Kredje, RookMeister,
noyou), and the author of Experience.
