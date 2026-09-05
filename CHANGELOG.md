# Changelog

Version 1.0.0
Adds the Levelling tab, which controls what a character level costs.
Adds the Skills tab, which shows every skill live and holds the cap, formula-cap and experience-rate settings.
Adds the Level Up tab for perk points and the attribute and carry-weight gains a level grants.
Adds the Static Levelling tab for a fixed experience amount per use of a skill.
Adds the Enchanting tab, which controls how the enchantment charge cost scales with the skill.
Adds the Presets tab, where a preset is a file you can share and each character remembers its own.
Adds the Patches tab, which lists each engine patch and states plainly whether it is active.
Adds a Debug tab with the log level and a live readout of the values the game is using.
Settings are stored in a plain INI file and can also be changed in game.

## 1.0.5 - 2026-09-05 - working

### Added
- Added a Skyrim 1.7.99 / 1.7.104 build; the mod now installs as a FOMOD that picks the build for your game version (SE 1.5.97 / AE 1.6.1170, or Skyrim 1.7.x). On 1.7.x the skill caps, formula caps and attribute gains attach; the skill experience rates, skill-to-level income, static levelling and perk-table sites are not present in that build and those groups stay off and say so.

### Fixed
- The Skill points group now says plainly when ordinary skill experience cannot be suppressed on the running build (skills then also advance by use).

## 1.0.4 - 2026-09-05 - working

### Added
- The cpc.control tool gained points, grantpoints, allocate and swf ops for headless testing of the level-up menu.
- Skill points: with Use skill points on, skills advance only by points spent in the level-up menu - each level grants points per level plus a multiplier times the level, banked when unspent, with cost tiers by skill level and a cap on increases per skill; the DLL feeds the menu and applies the allocation through the game's own skill-improve path, and the bank lives in the co-save. Ships Static Skill Leveling Rewritten's vanilla-look levelupmenu.swf with its authors' permission; its other skins fit the same contract.

### Fixed
- The shipped INI's perk-table key and the formula-cap note were stale; the INI now documents sPerksByLevel and says the formula cap is active.

## 1.0.3 - 2026-09-05 - working

### Added
- The cpc.control tool gained levelup, msgbox, givexp, static, peek and per-skill rate setters for headless testing.
- Static levelling is now active: each use of a skill pays a fixed share of a skill level, so a skill advances at the same rate at level 5 and level 50; perk bonuses to skill use are not applied while it is on.
- Attribute gains are now active: the health, magicka or stamina a level up grants, and the carry weight that comes with each choice, are this mod's values; the carry-weight cross terms stand down when Carryweight on Level Up or Carry Weight Per Level is loaded.
- Perk points are now granted from the by-level table: each level up grants the whole number of perks the table gives for the level reached (the fractional perks-per-level slider is gone).
- Skill-to-level income is now active: a skill increase pays vanilla times the Skill increase to level multiplier toward the character level; with Experience installed the group refuses its site and says so.
- Skill experience rates are now active: with Control skill experience rates on, each use of a skill pays vanilla times that skill's rate.

## 1.0.2 - 2026-09-05 - untested

### Added
- The formula cap is now active: with Control skill caps on, the value the game's own formulas read for a skill is held at fFormulaCap<Skill>, so a skill can advance past 100 while the combat maths treats it as its formula cap. The vanilla skills menu shows the capped value above the cap; this mod's Skills tab shows the true level.

## 1.0.1 - 2026-09-05 - untested

### Added
- The skills readout on the Skills tab and in the cpc.control tool now reports each skill's base and current actor value, the numbers the cap check actually uses.
- The skill cap patch is now active: with Control skill caps on, each skill stops advancing at the cap set on the Skills tab (Skyrim SE 1.5.97; the site is found by signature and proven by its 100.0 constant before anything is written; with the setting off nothing is written). The formula cap is not patched yet.
