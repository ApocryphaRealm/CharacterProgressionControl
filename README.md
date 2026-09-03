# Character Progression Control

One page for how a character grows: what a level costs, where experience comes from, skill caps
and rates, and what a level up grants. SKSE plugin, C++ with CommonLibSSE-NG, MIT, written from
scratch. Skyrim Special Edition.

**Version 0.1.0 - stage 1 of nine.** The plan is at `4. plans\character-progression-control`.

## What is in this build

- **Levelling tab** - what a character level costs. Skyrim works the cost of your next level out
  as `base + (per-level x your level)`, and this tab owns those two numbers. It needs no engine
  patch of any kind, which is why it is first.
- **Debug tab** - log level, and a live readout of what the game is actually using.
- Plain-file INI, the tabbed settings pages under the menu framework, and the `cpc.control`
  DevBench tool so the whole thing can be driven and read without a person at the keyboard.

Still to come, in the plan's order: skill caps and formula caps, experience rates and curves,
perks per level, attributes, static levelling, enchanting, presets, and alternative experience
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
