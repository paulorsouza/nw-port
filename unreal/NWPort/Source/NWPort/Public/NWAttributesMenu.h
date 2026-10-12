#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Aba de atributos da janela "Personagem" do jogo (Attributes de lyshineui/skills/skills.dynamicuicanvas,
 * tools/ui_build_attributes.py): runa com os pontos disponíveis, uma linha por atributo com valor, pendente
 * ("+N"), botões − e +, barra até 300 com marcas a cada 50, Confirmar e a barra de XP com o nível. Os pontos ficam
 * pendentes até Confirmar (o servidor confere em UNWProgressionComponent::ServerSpendPoints).
 */
class NWPORT_API FNWAttributesMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); Pending.Init(0, 5); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	void Click(const FVector2D& Mouse);

private:
	bool Load();
	FString Row(int32 i) const;
	int32 PendingTotal() const;
	FNWUILayout Layout;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	TArray<int32> Pending;
};
