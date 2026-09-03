// Character Progression Control - one page for how a character grows. Own code, MIT.
//
// Stage 1 (of the plan's nine): the plugin skeleton, the plain-file INI, the tabbed settings
// pages, the DevBench driving tool, and the Levelling tab - what a character level costs. That
// last one is deliberately the part that needs no engine patch at all, so the mod is useful and
// safe before a single hook exists.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Levelling.h"
#include "Settings.h"
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
			UI::Register();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Levelling::RequestApply();
			break;
		default:
			break;
		}
	}
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

	return true;
}
