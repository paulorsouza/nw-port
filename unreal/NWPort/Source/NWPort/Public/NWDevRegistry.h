#pragma once

#include "CoreMinimal.h"

class ANWPlayerController;

/**
 * Botões do menu de dev (F10) registrados por cada esteira no próprio arquivo (docs/esteiras.md), sem mexer no
 * ENWDevCommand, no ServerDev nem no SNWDevMenu. Server roda no servidor (com as mesmas regras do ServerDev); State,
 * se houver, mostra o estado no botão (no cliente).
 *
 *     NW_REGISTER_DEV(SummonHorse, TEXT("montarias.summon_horse"), TEXT("nw.dev.summon_horse"),
 *         [](ANWPlayerController* PC) { ... });
 *
 * Id único com o prefixo da esteira; rótulo é chave de texto (tools/strings/<esteira>.json).
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
