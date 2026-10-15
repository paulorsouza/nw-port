#pragma once

#include "CoreMinimal.h"

class ANWPlayerController;

/**
 * Dev menu (F10) buttons registered by each area in its own file (docs/workflow.md), without touching
 * ENWDevCommand, ServerDev or SNWDevMenu. Server runs on the server (with the same rules as ServerDev); State,
 * if present, shows the state on the button (on the client).
 *
 *     NW_REGISTER_DEV(SummonHorse, TEXT("montarias.summon_horse"), TEXT("nw.dev.summon_horse"),
 *         [](ANWPlayerController* PC) { ... });
 *
 * Unique id with the area prefix; the label is a text key (tools/strings/<area>.json).
 */
struct NWPORT_API FNWDevEntry
{
	FName Id;
	const TCHAR* LabelKey = nullptr;
	TFunction<void(ANWPlayerController*)> Server;
	TFunction<FText(const ANWPlayerController*)> State;
};

struct NWPORT_API FNWDevRegistry
{
	static TArray<FNWDevEntry>& All();
	static bool Add(FNWDevEntry Entry);
	static const FNWDevEntry* Find(FName Id);
};

#define NW_REGISTER_DEV(Name, Id, LabelKey, ...) \
	static const bool GNWDev_##Name = FNWDevRegistry::Add(FNWDevEntry{Id, LabelKey, __VA_ARGS__})
