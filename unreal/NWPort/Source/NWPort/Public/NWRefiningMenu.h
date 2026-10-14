#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class ANWStation;
class UCanvas;
struct FNWRecipe;
struct FNWItem;

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
	void Close() { Sound(TEXT("ui_crafting_outro")); Station.Reset(); Character.Reset(); }
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	/** true se o clique foi da tela (aberta, qualquer clique é dela). */
	bool Click(const FVector2D& Mouse);
	void Wheel(float Delta);
	/** A cada quadro com o botão esquerdo: arrastar o marcador da quantidade e repetir − e + enquanto segura. */
	void Hold(const FVector2D& Mouse, bool bDown);
	/** Espaço (ação tryskip do jogo): quantidade máxima na escolha, pula a contagem, volta à estação no fim. */
	void Space();
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

	// Botão segurado: marcador da barra ou − e + com repetição (button.lua OnTick).
	enum class EDrag : uint8 { None, Slider, Minus, Plus };
	EDrag Drag = EDrag::None;
	double HoldLast = 0.0;
	double HoldInitial = 0.0, HoldQuantity = 0.0, HoldPress = 0.0;
	int32 HoldStep = 1;
	void SliderTo(float MouseX);
	void Step(int32 Delta);

	enum class EState : uint8 { Select, Roll, Done };
	EState State = EState::Select;
	double RollStart = 0.0;
	int32 HaveBefore = 0;
	int32 RollBase = 0;
	// Arma/ferramenta: a contagem sobe até o GS do item novo (o espaço da bolsa que não tinha esse item antes).
	bool bGearRoll = false;
	TArray<int32> BeforeSlots;
	// Resultado do servidor (UNWInteractorComponent::CraftResult): a tela espera um Seq maior que WaitSeq.
	int32 WaitSeq = 0;
	uint8 RollKind = 255;   // 255 fabricar; senão ENWImprove
	FName RollItem;
	void StartResultRoll(FName ItemId, uint8 Kind);
	const FNWItem* RollResult() const;
	// Roll de equipamento (docs/analise-re/craftroll-equipamento.md): t0 = chegada do item; contagem do GS de GsFrom ao GS
	// do item em 1 s; atributo, perks e gema entram um a um.
	double ResultAt = 0.0;
	int32 GsFrom = 0;
	void DrawGearRoll(const FNWRecipe& R);
	/** Fim das animações do roll de equipamento (clique antes disso só termina as animações). */
	double GearRollLength() const;

	FNWUIElement* E(const TCHAR* Suffix) { return Ui.FindOne(TEXT("refining"), Suffix); }
	void Fill(UCanvas* Canvas, const FNWRecipe& R);
	void DrawRows(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawRoll(const FNWRecipe& R);
	void DrawHelp(UCanvas* Canvas);
	int32 MaxCount() const;
	double Now() const { return FixedNow > 0.0 ? FixedNow : FPlatformTime::Seconds(); }
	bool CanCraft(const FNWRecipe& R) const;

	// Fabricar arma, escudo ou ferramenta (docs/oficio.md): uma por vez, sem quantidade; faixa de GS e de qualidade, ou o
	// aviso de treino abaixo do nível de fabricar.
	void DrawCraftInfo(UCanvas* Canvas, const FNWRecipe& R, const FVector2D& Mouse);

	// Aba "Melhorar itens" (docs/itens.md): itens da bolsa deste ofício com sortear atributo, subir GS e desvincular.
	bool bReroll = false;
	FBox2f TabRect = FBox2f(ForceInit);
	struct FImproveButton { FBox2f Rect; int32 Slot = INDEX_NONE; uint8 Kind = 0; };
	TArray<FImproveButton> RerollButtons;
	void DrawReroll(UCanvas* Canvas, const FVector2D& Mouse);
	void DrawTab(UCanvas* Canvas, const FVector2D& Mouse);

	// Fim do roll de equipamento (craftroll-equipamento.md §8): Continuar, Equipar (item equipável na bolsa) e Sair da
	// estação, em coluna no ActiveButtonsContainer.
	enum class EDoneButton : uint8 { Continue, Equip, Exit };
	TArray<TPair<FBox2f, EDoneButton>> DoneButtons;
	void DrawDoneButtons(UCanvas* Canvas, const FVector2D& Mouse);
	// Sons da tela (craftroll-equipamento.md §9; Data/Audio/ui_sounds.json, tools/make_ui_sounds.py): uma variação
	// sorteada de cada camada do evento. SoundAt toca uma vez quando T passa de At (bit marca o que já tocou).
	void Sound(const TCHAR* Event) const;
	void SoundAt(double T, double At, int32 Bit, const FString& Event);
	uint32 SoundsDone = 0;
	/** Item do resultado na bolsa (o mesmo item que o servidor devolveu), ou nulo. */
	const FNWItem* ResultInBag() const;
};
