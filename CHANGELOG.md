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

## 1.0.1 - 2026-09-05 - untested

### Added
- The skills readout on the Skills tab and in the cpc.control tool now reports each skill's base and current actor value, the numbers the cap check actually uses.
- The skill cap patch is now active: with Control skill caps on, each skill stops advancing at the cap set on the Skills tab (Skyrim SE 1.5.97; the site is found by signature and proven by its 100.0 constant before anything is written; with the setting off nothing is written). The formula cap is not patched yet.
