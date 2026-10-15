#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NWPlayerGrid.generated.h"

class ANWCharacter;

/**
 * Jogadores perto de um ponto sem varrer o mundo (docs/tech/architecture.md, regra 1).
 *
 * Grade uniforme (células de CellSize) com os personagens que contam como jogador (ANWCharacter::IsPlayerLike: com
 * conexão ou simulado do teste de carga), refeita a cada RebuildSeconds com uma passada só pelos personagens. Quem
 * precisa de "jogadores a menos de R" (visão das criaturas, spawn, aggro) consulta só as células em volta.
 * Pode estar até RebuildSeconds atrasada: quem consulta confere a distância de novo.
 */
UCLASS()
class NWPORT_API UNWPlayerGrid : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float CellSize = 5000.f;       // 50 m
	static constexpr float RebuildSeconds = 0.25f;

	static UNWPlayerGrid* Get(const UWorld* World);
	/** Chama Fn para cada jogador vivo ou morto a menos de Radius (2D) de At. */
	void ForEachNear(const FVector& At, float Radius, TFunctionRef<void(ANWCharacter*)> Fn);
	/** Todos os jogadores (posição da última reconstrução). */
	const TArray<TWeakObjectPtr<ANWCharacter>>& All() { Refresh(); return Players; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	TArray<TWeakObjectPtr<ANWCharacter>> Players;
	TMap<FIntPoint, TArray<int32>> Cells;
	double BuiltAt = -1.0;
	void Refresh();
	static FIntPoint CellOf(const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize)); }
};
