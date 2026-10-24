// Off-game check of include/RaceShare.h - the share of carry weight and the starting attributes this mod owns (1.1.10:
// additive over the race by default, "use our base values" on request). The game is modelled as a permanent value made
// of the race's base, other mods' permanent modifiers and this mod's share; an apply moves the share by the difference,
// as CarryWeight.cpp and Attributes.cpp do.
//   tools\race_share_check.bat
#include "../include/RaceShare.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
	int g_pass = 0, g_fail = 0;

	void Check(bool a_ok, const std::string& a_what)
	{
		(a_ok ? g_pass : g_fail)++;
		std::cout << (a_ok ? "PASS " : "FAIL ") << a_what << "\n";
	}

	bool Near(float a, float b) { return std::fabs(a - b) < 0.01F; }

	// the player's carry weight: race base + other mods' permanent modifiers + this mod's share
	struct Game
	{
		float raceBase = 300.0F;
		float others = 0.0F;
		float ours = 0.0F;  // what the game holds of this mod's modifier
		float Permanent() const { return raceBase + others + ours; }
	};

	// one Apply(): the co-save's applied amount is the share this mod believes it holds
	void Apply(Game& a_g, float& a_applied, const raceshare::Carry& a_c)
	{
		const float share = raceshare::CarryShare(a_c, a_g.Permanent(), a_applied);
		const float delta = share - a_applied;
		if (std::fabs(delta) > 0.01F) { a_g.ours += delta; a_applied = share; }
	}
}

int main()
{
	using namespace raceshare;
	// (a) migration of a 1.1.9 INI
	Check(Near(OffsetFromStarting(320.0F), 20.0F) && Near(OffsetFromStarting(300.0F), 0.0F), "a 1.1.9 starting weight of 320 becomes an offset of 20 (300 -> 0)");

	// (b) a 250-base race (Altmer under Imperious) at level 5, offset 20, 5 per level: 250 + 20 + 5 x 4 = 290
	Game g;
	g.raceBase = 250.0F;
	float applied = 0.0F;
	Carry c;
	c.on = true;
	c.offset = 20.0F;
	c.perLevel = 5.0F;
	c.level = 5;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 290.0F) && Near(applied, 40.0F), "250-base race at level 5 -> 290 (this mod holds 40)");
	// save and load: the game kept the modifier, the co-save says 40
	Game loaded = g;
	float appliedLoaded = applied;
	Apply(loaded, appliedLoaded, c);
	Check(Near(loaded.Permanent(), 290.0F) && Near(appliedLoaded, 40.0F), "after save/load it stays 290 - nothing applied twice");
	// a fresh install on the same character (no co-save record): the share is added once
	Game fresh;
	fresh.raceBase = 250.0F;
	float appliedFresh = 0.0F;
	Apply(fresh, appliedFresh, c);
	Apply(fresh, appliedFresh, c);
	Check(Near(fresh.Permanent(), 290.0F), "a first install applies the share once, however often it runs");

	// (c) another source's modav carryweight 50 survives a level-up
	g.others += 50.0F;
	c.level = 6;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 345.0F) && Near(g.others, 50.0F), "another mod's +50 stays through a level-up (250 + 50 + 20 + 25)");
	// a race mod that changes the base mid-game (Human Spirit-style): seen at once, nothing recalculated over it
	g.raceBase += 10.0F;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 355.0F) && Near(applied, 45.0F), "a later change to the race's base is kept as it is");

	// (d) "use our base values" switched on and off mid-save
	c.ownBase = true;
	c.starting = 300.0F;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 325.0F), "our base values ON: the total is ours - 300 + 5 x 5 = 325, whatever the race and others set");
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 325.0F), "... and applying again changes nothing");
	c.level = 7;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 330.0F), "... and a level-up still adds the per-level gain (330 at level 7)");
	c.ownBase = false;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 260.0F + 50.0F + 20.0F + 30.0F) && Near(g.raceBase, 260.0F) && Near(g.others, 50.0F),
		  "OFF again: the race's base and the other mod's +50 are back exactly, with our additive share on top");

	// (e) the tab switched off: everything this mod added comes off
	c.on = false;
	Apply(g, applied, c);
	Check(Near(g.Permanent(), 310.0F) && Near(applied, 0.0F) && Near(g.ours, 0.0F), "control off: our share is taken away completely");

	// starting values OFF (the owner, 01:15): only the per-level growth, in either mode, and switching back is exact
	{
		Game h;
		h.raceBase = 250.0F;
		h.others = 50.0F;
		float ap = 0.0F;
		Carry k;
		k.on = true;
		k.offset = 20.0F;
		k.starting = 300.0F;
		k.perLevel = 5.0F;
		k.level = 5;
		Apply(h, ap, k);
		const float withStart = h.Permanent();  // 250 + 50 + 20 + 20 = 340
		k.startingValues = false;
		Apply(h, ap, k);
		Check(Near(h.Permanent(), 320.0F) && Near(ap, 20.0F), "starting values OFF, additive: only 5 x 4 on top of the race's 250 and the other +50");
		k.ownBase = true;
		Apply(h, ap, k);
		Check(Near(h.Permanent(), 320.0F), "starting values OFF, our base values: the race's and other mods' starting values kept, only our growth added");
		k.level = 6;
		Apply(h, ap, k);
		Check(Near(h.Permanent(), 325.0F), "... a level-up adds our per-level gain");
		k.startingValues = true;
		Apply(h, ap, k);
		Check(Near(h.Permanent(), 325.0F), "starting values back ON with our base values: the total is ours, 300 + 5 x 5");
		k.ownBase = false;
		Apply(h, ap, k);
		Check(Near(h.Permanent(), withStart + 5.0F) && Near(h.raceBase, 250.0F) && Near(h.others, 50.0F),
			  "everything back to additive with starting values: exactly as before, plus the one level gained");
	}

	// starting attributes: additive over the race's start, or the race's start replaced; nothing with starting values off
	Check(Near(AttributeShare(true, true, false, 120.0F, 80.0F), 20.0F), "attribute 120, additive: +20 on top of the race's start (80 -> 100)");
	Check(Near(AttributeShare(true, true, true, 120.0F, 80.0F), 40.0F), "attribute 120, our base values: the race's 80 is replaced (80 + 40 = 120)");
	Check(Near(AttributeShare(true, false, false, 120.0F, 80.0F), 0.0F) && Near(AttributeShare(true, false, true, 120.0F, 80.0F), 0.0F),
		  "attribute, starting values off: the race's start is left alone in both modes");
	Check(Near(AttributeShare(false, true, true, 120.0F, 80.0F), 0.0F), "attributes off: nothing");

	std::cout << "race_share_check: " << g_pass << " of " << (g_pass + g_fail) << " passed\n";
	return g_fail ? 1 : 0;
}
