#pragma once

#include "CoreMinimal.h"
#include "NWUILayout.h"

class ANWCharacter;
class UCanvas;

/**
 * Tela de maestria de armas do jogo (MasteryTreeWindow de lyshineui/skills/skills.dynamicuicanvas), desenhada
 * por FNWUILayout. O que o Lua do jogo fazia vem daqui: nós nas linhas/colunas do datasheet, estados
 * (ativo, passivo, ultimate; possuído/disponível), espaços Q/R/F, pontos e tooltip.
 *
 * Clique numa habilidade ativa e depois num espaço para equipar. Passivas e ultimate gastam pontos (só visual
 * por enquanto). Abas 1/2 no cabeçalho para a outra arma.
 */
class NWPORT_API FNWMasteryMenu
{
public:
	bool Load();
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter);
	void Close() { Character.Reset(); Pending = NAME_None; }

	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	// true = clique consumido; bCloseRequested = fechar a tela.
	bool Click(const FVector2D& Mouse, bool& bCloseRequested);

private:
	struct FNode
	{
		FString Id;
		FName Action;
		FString Kind;        // active, passive, ultimate
		int32 Tree = 0, Row = 0, Col = 0;
		FString Icon, Background, Name, Description, Parent;
		float Cooldown = 0.f;
		FString ElementPath; // caminho do nó na tela (…/Row2/Node3)
		// Elementos do nó na tela (fixos depois do Load).
		FNWUIElement *Root = nullptr, *Rune = nullptr, *Glow = nullptr, *Bg = nullptr, *Border = nullptr, *IconBg = nullptr,
			*IconImg = nullptr, *VLine = nullptr, *HLine = nullptr, *Ultimate = nullptr, *Lock = nullptr, *Ticks = nullptr;
	};
	struct FWeapon
	{
		FString Key, Title, HudIcon;
		TArray<FString> Trees;
		TArray<FNode> Nodes;
	};

	void ApplyWeapon();
	void Refresh();
	int32 SpentInTree(int32 Tree) const;
	int32 PointsLeft() const;
	TArray<FName> Slots() const;
	FNode* NodeAt(const FVector2D& Mouse);
	void DrawTooltip(UCanvas* Canvas, const FNode& N, const FVector2D& Mouse);
	void LoadOwned();
	void SaveOwned();
	FNWUIElement* E(const FString& Suffix) { return Layout.FindOne(TEXT("mastery"), Suffix); }

	FNWUILayout Layout;
	TArray<FWeapon> Weapons;
	int32 Tab = 0;
	TWeakObjectPtr<ANWCharacter> Character;
	FName Pending;                 // habilidade ativa escolhida, esperando um espaço
	TSet<FString> Owned;           // passivas/ultimate marcadas (por arma)
	TArray<FBox2f> TabRects;
	float UIScale = 1.f;
	bool bHoverConfirm = false;
	static constexpr int32 TotalPoints = 19;      // pontos de maestria no nível 20 do jogo
	static constexpr int32 UltimatePoints = 10;   // pontos na árvore para liberar o ultimate
};
