#include "PCH.h"

#include "GameSettings.h"

#include "utils/Logger.h"

namespace GameSettings
{
	namespace
	{
		RE::Setting* Find(const char* a_name)
		{
			auto* collection = RE::GameSettingCollection::GetSingleton();
			return collection ? collection->GetSetting(a_name) : nullptr;
		}
	}

	void Group::Add(const char* a_name, float* a_value)
	{
		Entry e;
		e.name = a_name;
		e.value = a_value;
		entries.push_back(e);
	}

	void Group::Capture()
	{
		if (captured) { return; }
		captured = true;

		for (auto& e : entries)
		{
			if (auto* setting = Find(e.name))
			{
				e.vanilla = setting->GetFloat();
				e.found = true;
			}
			else
			{
				// Not an error: a runtime that does not have this setting simply cannot be
				// configured here, and the tab will say so rather than silently doing nothing.
				e.found = false;
				logger::warn("{}: game setting {} does not exist on this runtime; it will be left alone",
							 label, e.name);
			}
		}
		logger::info("{}: captured {} of {} game settings from this install",
					 label, FoundCount(), static_cast<int>(entries.size()));
	}

	void Group::Apply(bool a_active)
	{
		if (!captured) { return; }

		for (const auto& e : entries)
		{
			if (!e.found || !e.value) { continue; }
			auto* setting = Find(e.name);
			if (!setting) { continue; }
			setting->data.f = a_active ? *e.value : e.vanilla;
		}
		logger::debug("{}: {}", label, a_active ? "applied our values" : "restored this install's own values");
	}

	int Group::FoundCount() const
	{
		int n = 0;
		for (const auto& e : entries) { if (e.found) { ++n; } }
		return n;
	}

	float Group::Live(int a_index) const
	{
		if (a_index < 0 || a_index >= static_cast<int>(entries.size())) { return 0.0F; }
		auto* setting = Find(entries[static_cast<std::size_t>(a_index)].name);
		return setting ? setting->GetFloat() : 0.0F;
	}

	void Group::SeedFromVanilla()
	{
		for (const auto& e : entries)
		{
			if (e.found && e.value) { *e.value = e.vanilla; }
		}
	}
}
