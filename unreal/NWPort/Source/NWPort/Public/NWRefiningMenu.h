#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class ANWStation;
class UCanvas;
struct FNWRecipe;

/**
 * Tela de refino de uma estação (esteira 4) com o layout do jogo: lyshineui/crafting/craftingscreenv4 no modo refino
 * (tools/ui_screens/refining.json; o que liga, desliga e preenche: docs/hud/refino.md), a barra de título de
 * stationproperties (screen_header.json) e o rodapé de atalhos (helpbar.json).
 * Estados: escolha da receita; roll (contagem do produto, 1 s, craftroll.lua); fim (clique continua).
 */
class NWPORT_API FNWRefiningMenu
{
public:
	bool IsOpen() const { return Station.IsValid() && Character.IsValid(); }
	void Open(ANWCharacter* InCharacter, ANWStation* InStation);
	void Close() { Station.Reset(); Character.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** true se o clique foi da tela (aberta, qualquer clique é dela). */
	bool Click(const FVector2D& Mouse);
	void Wheel(float Delta);
	/** Clique em CONFECCIONAR (também usado pela captura offline). */
	void ClickCraft();
	/** Escolhe a receita e a quantidade (captura offline). */
	void Select(FName RecipeId, int32 InCount);
	/** Retângulos e estado dos elementos cujo caminho contém Filter, no último desenho (captura offline). */
	void Dump(const FString& Filter) const;
	/** Relógio fixo para a captura offline (NWUIShotCommandlet); 0 = relógio real. */
	double FixedNow = 0.0;

private:
	bool Load();
	FNWUILayout Ui, Head, Help;
	bool bLoaded = false;
	TWeakObjectPtr<ANWCharacter> Character;
	TWeakObjectPtr<ANWStation> Station;
	TArray<const FNWRecipe*> Recipes;

	// Árvore: 0 aprendidas, 1 família, 2 grupo, 3 receita (craftingtreeviewitem.lua).
	struct FRow { int32 Depth = 0; FString Text; FString Key; int32 Recipe = INDEX_NONE; };
	TArray<FRow> Rows;
	TSet<FString> Collapsed;
	void BuildRows();
	TArray<TPair<FBox2f, int32>> RowRects;
	int32 Selected = 0;
	int32 Scroll = 0;
	int32 Count = 1;

	enum class EState : uint8 { Select, Roll, Done };
	EState State = EState::Select;
	double RollStart = 0.0;
	int32 HaveBefore = 0;
	int32 RollBase = 0;

	FNWUIElement* E(const TCHAR* Suffix) { return Ui.FindOne(TEXT("refining"), Suffix); }
	void Fill(UCanvas* Canvas, const FNWRecipe& R);
	void DrawRows(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawRoll(const FNWRecipe& R);
	void DrawHelp(UCanvas* Canvas);
	int32 MaxCount() const;
	double Now() const { return FixedNow > 0.0 ? FixedNow : FPlatformTime::Seconds(); }
	bool CanCraft(const FNWRecipe& R) const;
};
