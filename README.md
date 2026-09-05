# Character Progression Control

Version 1.0.3

One page for how a character grows: what a level costs, where experience comes from, skill caps
and rates, and what a level up grants. SKSE plugin, C++ with CommonLibSSE-NG, MIT, written from
scratch. Skyrim Special Edition.

**Version 1.0.3 - stages 1 to 8 of nine, every engine patch verified in game on Skyrim SE 1.5.97
(2026-09-05).** The plan is at `4. plans\character-progression-control`. Stage 9 (alternative
experience sources) is a separate decision and is not started.

What each patch group does, and how it was proven, is on the Patches tab in game and in the
source comments; the short version: the skill cap and formula cap (Skills tab), the skill
experience rates and skill-to-level income (Skills tab), the perk table and attribute gains with
their carry-weight cross terms (Level Up tab), and static levelling (its own tab) are all live.
Every site is found by its byte shape in the running game, matched exactly once, proven by what
it loads or compares before a byte is written, and with its setting off nothing is written at all.

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
