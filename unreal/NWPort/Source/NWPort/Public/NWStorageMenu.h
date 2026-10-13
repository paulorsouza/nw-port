#pragma once

#include "CoreMinimal.h"

class ANWCharacter;
class UCanvas;

/**
 * Tela do armazém da cidade (esteira 4): bolsa à esquerda, despensa à direita, peso de cada lado; clique numa linha
 * move a pilha inteira (o jogo: duplo clique ou Shift + clique, container/catcontainer.lua). Provisória, nas peças da
 * tela de refino, até o padrão das telas da pesquisa de HUD.
 */
class NWPORT_API FNWStorageMenu
{
public:
	bool IsOpen() const { return Character.IsValid(); }
	void Open(ANWCharacter* InCharacter, const FString& InTitle);
	void Close();
	void Draw(UCanvas* Canvas, const FVector2D& Mouse);
	bool Click(const FVector2D& Mouse);
	void Wheel(float Delta, const FVector2D& Mouse);

private:
	TWeakObjectPtr<ANWCharacter> Character;
	FString Title;
	int32 ScrollBag = 0, ScrollStore = 0;
	float S = 1.f;
	FBox2f Origin;
	TArray<TPair<FBox2f, int32>> BagRows, StoreRows;
	FVector2D P(float X, float Y) const { return FVector2D(Origin.Min.X + X * S, Origin.Min.Y + Y * S); }
	FBox2f R(float X0, float Y0, float X1, float Y1) const { return FBox2f(FVector2f(P(X0, Y0)), FVector2f(P(X1, Y1))); }
	void Text(UCanvas* Canvas, const FString& T, float X, float Y, float Size, const FLinearColor& Color, int32 Style = 1, bool bRight = false) const;
	void Tex(UCanvas* Canvas, const FString& Path, const FBox2f& Box, const FLinearColor& Tint = FLinearColor::White) const;
	void Column(UCanvas* Canvas, const FVector2D& Mouse, float X, const FString& Header, const TArray<const struct FNWItem*>& Items,
		float Weight, float Capacity, int32& Scroll, TArray<TPair<FBox2f, int32>>& Rows);
};
