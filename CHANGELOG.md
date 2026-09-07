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

## 1.1.2 - 2026-09-06 - working

### Fixed
- The Difficulty tab wrote Skyrim's compiled vanilla damage multipliers over whatever the game loaded with whenever its damage control was OFF - on every load. Beside Blade and Blunt or Requiem that silently reverted their tuning. Off now writes nothing: the twelve values are captured at data load, after every plugin's records, and switching a control off hands those loaded values back once. The regeneration path follows the same rule.

### Added
- One pair for every difficulty (Yet Another Difficulty Mod's Simple mode): a switch and a single pair written for all six difficulties.
- Difficulty by level (its Dynamic mode): a switch and six level thresholds; on a save load and on every level-up the highest difficulty whose level the player has reached becomes the game's difficulty, through the same path the Settings menu uses. Defaults are Blade and Blunt's milestones, one tier per ten levels.
- The built-in Blade and Blunt / Requiem patch: both plugins are detected, the page says their values are the loaded values and that this tab writes last while a control is on, and three presets fill the table - the loaded values, Blade and Blunt's pairs, Requiem's (all 1.0) - plus vanilla. Every write goes through the task queue after the other plugins' handlers and is repeated on the level-up event, so this mod supersedes theirs. Blade and Blunt's INI is read and the page asks for bLevelBasedDifficulty = false while it is present.
- The damage sliders run 0 to 999 in 0.01 steps (Ctrl+click to type); the loaded value is shown beside every pair.
- cpc.control: shared, sharedto, sharedby, bylevel, levelfor<d>, checklevel and preset ops; difficultyvalues reports the loaded values, the level table and the overhaul detection.

## 1.1.1 - 2026-09-06 - working

### Added
- THE EXPERIENCE TAB - stage 9, alternative experience sources, written from scratch on the game's own story events. With Earn experience from quests, exploration and kills on (off by default; bAlternativeExperience in the INI), character experience comes from quests completed (an amount per kind: main, faction, Daedric, side, miscellaneous, other), locations discovered, locations cleared, kills (a base amount plus an amount per level of the victim; kills by followers and summons optionally) and books read (skill books separately) - alongside skill use, or, with Skill increases still pay toward your level off, instead of it (the level-income patch then returns nothing for a skill increase; attaches at startup). When a grant crosses the next level's cost the game's own level-up notice is shown. What each character has earned from each source is kept in the co-save and read out on the tab. Stands down while the Experience mod is loaded. The cpc.control tool gained op=experience, op=experience:<0|1>, op=skillspay:<0|1> and "xpsim":"quest|location|cleared|kill|book".

### Fixed
- The skill-to-level income patch's call stub now preserves every register the game's call site relies on across the hook (rcx, rdx, r8-r11, xmm1-xmm5): the site adds the returned experience to the field rcx addresses the instruction after the call, and a hook that touched rcx either crashed the game on the first skill increase or, when rcx happened to land on readable memory, wrote the experience somewhere else, so a skill increase paid nothing toward the level with the patch attached. Measured on SE 1.5.97: replace mode pays 0, supplement mode and the tab off pay exactly the vanilla amount, and the hook may log again.

## 1.1.0 - 2026-09-05 - working

### Added
- THE DIFFICULTY TAB - Custom Difficulty UI's mechanic inside this mod (Custom Difficulty UI stays a separate minimal mod; the tab stands down while it is loaded). Damage: with Control damage multipliers on (off by default; bControlDamage in the INI) each difficulty's pair of vanilla multipliers - damage dealt to you and by you - is written, and the difficulty you play on picks the pair; off restores Skyrim's own values. Regeneration: vanilla has one set of regeneration values for every difficulty; with Control regeneration on (bControlRegeneration) each difficulty has its own set of the seven in-combat rate and after-damage delay settings plus five global ones, the set for the difficulty you play on is written into the game, and it is re-written the moment the difficulty changes in the game's Settings (the Journal Menu closing - nothing polls); the values your game came with are captured on first load and restored when off. The cpc.control tool gained op=difficultyvalues, op=damage:<0|1>, op=regen:<0|1>, op=dmgto<d>:<v>, op=dmgby<d>:<v> and op=regenhp<d>:<v>.

## 1.0.9 - 2026-09-05 - working

### Added
- THE CARRY WEIGHT TAB. Carryweight on Level Up's shape, now a tab here: with Control carry weight on (off by default; bControlCarryWeight in the INI) your permanent carry weight is starting + per level x (level - 1), recalculated when a save loads, when you level up (the level event and the attribute choice itself), when a value changes and on Apply now - nothing polls. The net amount added is kept in the co-save, so turning it off takes exactly that away again. While it is on, the per-choice carry weight is not applied (the formula sets the total), and it stands down while Carryweight on Level Up or Carry Weight Per Level is loaded. The cpc.control tool gained op=carryweight, op=carryweight:<0|1>, op=cwstart:<n>, op=cwperlevel:<n> and op=cwapply.
- THE ATTRIBUTES TAB. Starting health, magicka and stamina (Control starting attributes, off by default; bControlStartingAttributes and fStartingHealth/Magicka/Stamina), applied as (value - 100) on top of your race's own start as this mod's permanent modifier and taken away again when turned off; the per-level gains, moved here from the Level Up tab; and a live readout, race start + this mod + per level x invested. The game keeps no count of attribute choices, so this mod counts every one itself from the moment it is installed on a character (co-save) and says from which level the count began instead of guessing at earlier ones. The cpc.control tool gained op=attributes, op=attributes:<0|1> and op=starthealth/startmagicka/startstamina:<n>.

### Changed
- The attribute level-up patch attaches whenever its site is found, because it is the counter, and Control what a level up grants no longer needs a restart: while it is off the patch hands the game its own numbers - carry weight on the stamina choice only - exactly as vanilla.
- The Level Up tab keeps the perk table and the master switch; the attribute and carry-weight sliders live on the two new tabs.
- The two permanent modifiers are checked on load: the game keeps the health/magicka/stamina one across a save but drops the carry-weight one (measured on SE 1.5.97), so the co-save also carries the permanent value as it stood after the last apply, and the first apply after a load either carries on or starts the applied amount from zero - no drift either way, and switching off always takes away exactly what is there.
- The cpc.control tool gained op=save, which writes the INI.

## 1.0.8 - 2026-09-05 - working

### Added
- ONE CONFIGURATION PER GAME DIFFICULTY. A new toggle on the Presets page, Follow the game's difficulty (off by default; bFollowDifficulty in the INI): while it is on, the difficulty set in the game's own Settings decides which of six presets is in use - Difficulty - Novice, Apprentice, Adept, Expert, Master and Legendary, ordinary preset files in the Presets folder - and changing the difficulty switches them: what you had is saved into the old difficulty's preset and the new one's is loaded, created from the current configuration the first time that difficulty is met. The switch happens when the Journal Menu closes (where the game's Settings live) and when a save loads; nothing polls. The cpc.control tool gained op=difficulty, op=difficulty:<0-5> and op=follow:<0|1>.

## 1.0.7 - 2026-09-05 - working

### Fixed
- A respec now also rewrites each reset skill's cached level and next-level threshold, so the first level after it costs what that level costs and not what the old, higher level cost.

## 1.0.6 - 2026-09-05 - working

### Added
- A respec of the skill points: any mod can send the SKSE mod event CPC_RefundSkillPoints (Potion of Clarity does) and every skill above its starting value goes back to it, its experience clears, and the points this mod's cost tiers charged for the levels above come back to the bank; the cpc.control tool gained a refund op.

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
