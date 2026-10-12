#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Aba de ofícios de coleta da janela "Personagem" do jogo (TradeSkills > GatheringSection de
 * lyshineui/skills/skills.dynamicuicanvas, tools/ui_build_tradeskills.py): uma célula por ofício com o nível no
 * meio do círculo e o progresso até o próximo nível no anel. Abre pela aba no topo da janela K.
 */
class NWPORT_API FNWTradeskillMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);

	/**
	 * Abas da janela K (Maestria em Armas / Hab. Mercantes), desenhadas por cima da tela aberta; Active = aba
	 * atual. Devolve a aba sob o mouse no clique (ou INDEX_NONE) por TabAt.
	 */
	static void DrawTabs(UCanvas* Canvas, const FVector2D& Mouse, int32 Active);
	static int32 TabAt(const FVector2D& Mouse);

private:
	bool Load();
	FNWUILayout Layout;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	static FBox2f TabRects[2];
};
