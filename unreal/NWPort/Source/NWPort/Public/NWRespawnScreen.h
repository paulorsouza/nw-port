#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class UCanvas;
class UNWCombatComponent;

/**
 * Game death screen (Data/UI/respawn_layout.json, tools/ui_build_respawn.py): "Morto Por" with the name, counter
 * and the Respawn button, which unlocks when the server allows it (UNWCombatComponent::RespawnAt).
 */
class NWPORT_API FNWRespawnScreen
{
public:
	void Draw(UCanvas* Canvas, const FVector2D& Mouse, const UNWCombatComponent& Combat);
	// true = clicked Respawn with the button unlocked.
	bool Click(const FVector2D& Mouse, const UNWCombatComponent& Combat);

private:
	bool Load();
	FNWUIElement* E(const FString& Suffix) { return Layout.FindOne(TEXT("respawn"), Suffix); }
	FString Str(const TCHAR* Key) const;

	FNWUILayout Layout;
	bool bLoaded = false;
	bool bTried = false;
};
