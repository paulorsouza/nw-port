#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NWLoadTest.generated.h"

class ANWCharacter;

/**
 * Teste de carga do servidor (docs/tech/architecture.md, "Teste de carga").
 *
 * - Jogadores simulados: `nw.sim <n> [raio_m]` (ou `-nwsim=<n> -nwsimraio=<m>` na linha de comando do servidor) cria n
 *   personagens no servidor, sem conexão, com controle de IA e marcados como jogador (ANWCharacter::bSimPlayer: o spawn
 *   de criaturas e a IA os tratam como jogador). Papéis: 30% andam na cidade (40 m do início), 50% andam pelo mapa
 *   (até o raio), 20% andam e atacam. `nw.sim 0` remove todos.
 * - Relatório: `nw.perf 1` (ou `-nwperf`) grava a cada 10 s no log (`NWPerf:`) e em Saved/Perf/perf_<data>.csv o tempo
 *   de trabalho do ciclo do mundo (média, p95, máximo, sem a espera do limite de 30/s), o intervalo real entre ciclos,
 *   jogadores conectados, simulados, criaturas, personagens, bytes enviados por segundo e memória.
 * - Perfil: `-nwcsv=<ciclos>` grava o perfil CSV da Unreal (tempo por categoria em cada ciclo) `-nwcsvdelay=<s>` (padrão
 *   15) depois de criar os simulados, em Saved/Profiling/CSV (`tools/perf_csv.py` resume).
 * - Experimentos: `-nwexp=meshcol,floor` muda uma configuração em todos os personagens só no teste, para medir antes
 *   de levar ao jogo (meshcol: malha sem colisão; floor: menos checagem de chão).
 *
 * O próprio teste custa pouco (um passo por simulado a cada ciclo, sem varrer o mundo), mas entra na medição.
 */
UCLASS(NotPlaceable, Transient)
class NWPORT_API ANWLoadTest : public AActor
{
	GENERATED_BODY()

public:
	ANWLoadTest();

	/** Servidor: deixa n simulados no mundo (cria ou remove), espalhados até RadiusM metros do início. */
	static void SetSims(UWorld* World, int32 Count, float RadiusM);
	/** Liga ou desliga o relatório de desempenho do mundo. */
	static void SetPerf(UWorld* World, bool bOn);

	virtual void Tick(float DeltaSeconds) override;

private:
	enum class ERole : uint8 { City, Roam, Fight };
	struct FSim
	{
		TWeakObjectPtr<ANWCharacter> Pawn;
		ERole Role = ERole::Roam;
		FVector Goal = FVector::ZeroVector;
		FVector LastPos = FVector::ZeroVector;
		double NextCheck = 0.0;
		double NextAttack = 0.0;
	};
	TArray<FSim> Sims;
	FVector Anchor = FVector::ZeroVector;
	float Radius = 60000.f;
	bool bHasAnchor = false;

	static ANWLoadTest* Get(UWorld* World, bool bCreate);
	void Resize(int32 Count);
	bool FindGround(const FVector& Around, float R, FVector& Out) const;
	void PickGoal(FSim& S);
};
