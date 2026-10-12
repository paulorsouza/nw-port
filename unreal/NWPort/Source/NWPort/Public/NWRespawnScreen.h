#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class UCanvas;
class UNWCombatComponent;

/**
 * Tela de morte do jogo (Data/UI/respawn_layout.json, tools/ui_build_respawn.py): "Morto Por" com o nome, contador
 * e o botão Renascer, que libera quando o servidor permite (UNWCombatComponent::RespawnAt).
 */
class NWPORT_API FNWRespawnScreen
{
public:
	void Draw(UCanvas* Canvas, const FVector2D& Mouse, const UNWCombatComponent& Combat);
	// true = clicou em Renascer com o botão liberado.
	bool Click(const FVector2D& Mouse, const UNWCombatComponent& Combat);

private:
	bool Load();
	FNWUIElement* E(const FString& Suffix) { return Layout.FindOne(TEXT("respawn"), Suffix); }
	FString Str(const TCHAR* Key) const;

	FNWUILayout Layout;
	bool bLoaded = false;
	bool bTried = false;
};
