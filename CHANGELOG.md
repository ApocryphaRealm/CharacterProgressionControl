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
