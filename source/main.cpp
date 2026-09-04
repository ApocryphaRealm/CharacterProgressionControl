// Character Progression Control - one page for how a character grows. Own code, MIT.
//
// Stage 1 (of the plan's nine): the plugin skeleton, the plain-file INI, the tabbed settings
// pages, the DevBench driving tool, and the Levelling tab - what a character level costs. That
// last one is deliberately the part that needs no engine patch at all, so the mod is useful and
// safe before a single hook exists.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Compat.h"
#include "Enchanting.h"
#include "Levelling.h"
#include "Patches.h"
#include "Presets.h"
#include "Settings.h"
#include "Skills.h"
#include "UI.h"

#include "utils/Logger.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			// Read the game's own values BEFORE anything is written, so "vanilla" means what
			// this installation actually had.
			Levelling::CaptureVanilla();
			Enchanting::CaptureVanilla();
			// Register every patch group first, then install them in one pass so each one's
			// outcome is logged together and a failure is a reported fact, not a crash.
			Compat::Detect();
			Skills::Register();
			Patches::InstallAll();
			UI::Register();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// The co-save has been read by this point, so THIS character's preset selection is
			// known, and applying it is what sets every value for this save.
			Presets::ApplySelection();
			Levelling::RequestApply();
			Enchanting::RequestApply();
			break;
		default:
			break;
		}
	}

	void SaveCallback(SKSE::SerializationInterface* a_intfc) { Presets::OnSave(a_intfc); }
	void LoadCallback(SKSE::SerializationInterface* a_intfc) { Presets::OnLoad(a_intfc); }
	void RevertCallback(SKSE::SerializationInterface* a_intfc) { Presets::OnRevert(a_intfc); }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SKSE::log::init("CharacterProgressionControl");

	settings::Init("CharacterProgressionControl.ini");
	settings::ApplyLogLevel();

	logger::info("Character Progression Control {} loading",
				 SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	// Preset CONTENTS are shared files; which preset a character is on lives in that character's
	// co-save, so two saves can sit on different presets at once.
	if (auto* serialization = SKSE::GetSerializationInterface())
	{
		serialization->SetUniqueID('CPCS');
		serialization->SetSaveCallback(SaveCallback);
		serialization->SetLoadCallback(LoadCallback);
		serialization->SetRevertCallback(RevertCallback);
	}

	return true;
}
