#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class ANWCharacter;

/**
 * Teclas registradas por cada esteira no próprio arquivo (docs/esteiras.md), sem mexer no ANWCharacter nem no
 * FNWOptions. Entram na aba Atalhos (trocáveis) e são ligadas no SetupPlayerInputComponent do personagem.
 *
 *     NW_REGISTER_INPUT(MountSummon, TEXT("MountSummon"), TEXT("ui_mount_summon"), EKeys::T,
 *         [](ANWCharacter* C) { C->Mount->Summon(); });
 *
 * O id é o da tecla nas opções; o rótulo é uma chave de texto (tools/strings/<esteira>.json ou do jogo).
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
