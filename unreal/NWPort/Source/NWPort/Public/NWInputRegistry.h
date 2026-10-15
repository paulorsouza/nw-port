#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class ANWCharacter;

/**
 * Keys registered by each area in its own file (docs/workflow.md), without touching ANWCharacter or
 * FNWOptions. They go into the Shortcuts tab (rebindable) and are bound in the character's SetupPlayerInputComponent.
 *
 *     NW_REGISTER_INPUT(MountSummon, TEXT("MountSummon"), TEXT("ui_mount_summon"), EKeys::T,
 *         [](ANWCharacter* C) { C->Mount->Summon(); });
 *
 * The id is the key's id in the options; the label is a text key (tools/strings/<area>.json or from the game).
 */
struct NWPORT_API FNWInputBinding
{
	FName Id;
	const TCHAR* LabelKey = nullptr;
	FKey Default;
	TFunction<void(ANWCharacter*)> Pressed;
	TFunction<void(ANWCharacter*)> Released;
};

struct NWPORT_API FNWInputRegistry
{
	static TArray<FNWInputBinding>& All();
	static bool Add(FNWInputBinding Binding);
};

#define NW_REGISTER_INPUT(Name, Id, LabelKey, DefaultKey, ...) \
	static const bool GNWInput_##Name = FNWInputRegistry::Add(FNWInputBinding{Id, LabelKey, DefaultKey, __VA_ARGS__})
