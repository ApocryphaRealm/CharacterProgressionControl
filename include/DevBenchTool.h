#pragma once

namespace DevBenchTool
{
	// Registers "cpc.control" with DevBench when present (the driving-tool standard, so the mod
	// can be driven and read without a person at the keyboard).
	// Call with false at kPostLoad and true at kDataLoaded.
	void Init(bool a_lastAttempt);
}
