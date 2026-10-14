#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWCombatTypes.h"
#include "NWBehaviorTreeComponent.generated.h"

class ANWCharacter;
class FJsonObject;

/** Nó da árvore do jogo (Data/AI/<conjunto>.json, tools/make_ai.py). */
struct FNWBTNode
{
	FName Type;
	FString Name;
	float Utility = 0.f;
	bool bNonInterruptable = false;
	TSharedPtr<FJsonObject> P;
	TArray<int32> Children;
};

/** Árvore e perfil de uma criatura, carregados uma vez por conjunto. */
struct FNWBTAsset
{
	TArray<FNWBTNode> Nodes;
	TSharedPtr<FJsonObject> Profile;
	TSharedPtr<FJsonObject> Tasks;
	static const FNWBTAsset* Get(FName Set);
};

/** Valor do blackboard: número, vetor ou texto. */
struct FNWBBValue
{
	double Num = 0.0;
	FVector Vec = FVector::ZeroVector;
	FString Str;
	bool bVec = false;
};

/**
 * IA da criatura igual à do jogo: interpreta a árvore de comportamento do Server.pak (docs/analise-re/ia-mobs.md).
 * Só no servidor.
 *
 * Planejar (Evaluate): seletor pega o primeiro filho que avalia; sequência exige todos; UtilitySelector escolhe por
 * utilidade (maior ou sorteio proporcional, mantendo a escolha enquanto ela avaliar); condições testam o grupo de
 * entrada; recarga e limitador de frequência falham enquanto ativos. Executar: folhas rodam a cada tick até sucesso
 * ou falha; condições checam continuação (falhou = termina com sucesso) e interrupção (falhou = falha); recarga
 * começa quando o filho termina. Replaneja a cada `replan_seconds` ou quando muda uma chave observada, exceto
 * enquanto um nó não interrompível está no caminho em execução.
 *
 * O que no jogo é do servidor e foi cortado do cliente (sensor de alvo, movimento) é feito aqui: alvo pela visão do
 * slice (ambiente/combate), dano recebido e aggro de grupo; movimento em linha reta com a velocidade e o giro da
 * configuração de navegação do slice.
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWBehaviorTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWBehaviorTreeComponent();

	/** Liga a árvore do conjunto (wolf, boar). Falso se não houver Data/AI/<conjunto>.json. */
	bool Start(FName Set, const FVector& Home, const TArray<FVector>& Waypoints);
	ANWCharacter* GetTarget() const { return Target.Get(); }
	/** Aggro vindo de fora (grupo, dano). */
	void Aggro(ANWCharacter* Who);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EStatus : uint8 { Running, Success, Failure };

	// Plano: árvore dos nós escolhidos, com o estado de execução de cada um.
	struct FPlan
	{
		int32 Node = INDEX_NONE;
		TArray<TSharedPtr<FPlan>> Kids;
		int32 Cursor = 0;
		bool bStarted = false;
		double StartTime = 0.0;
		double Duration = 0.0;
		FVector Dest = FVector::ZeroVector;
		FVector LastTargetPos = FVector::ZeroVector;
		bool bActionRequested = false;
		float Utility = 0.f;
	};

	// Planejar e executar.
	TSharedPtr<FPlan> Evaluate(int32 Node);
	EStatus Execute(FPlan& Plan, float Dt);
	EStatus ExecuteLeaf(FPlan& Plan, const FNWBTNode& N, float Dt);
	void Abort(FPlan& Plan);
	void Signature(const FPlan& Plan, TArray<int32>& Out) const;
	bool RunningPathNonInterruptable(const FPlan& Plan) const;

	// Condições e parâmetros.
	bool CheckGroup(const FNWBTNode& N, const TCHAR* Group, bool bDefault) const;
	bool HasGroup(const FNWBTNode& N, const TCHAR* Group) const;
	double Operand(const TSharedPtr<FJsonObject>& O, bool bDistance) const;
	double Num(const FNWBTNode& N, const TCHAR* Key, double Default) const;
	FString Str(const FNWBTNode& N, const TCHAR* Key) const;
	bool Bool(const FNWBTNode& N, const TCHAR* Key, bool Default) const;
	FVector PositionOf(int32 PositionType) const;
	bool SelectPosition(const TSharedPtr<FJsonObject>& Params, const FString& Prefix, FVector& Out) const;
	double NumIn(const TSharedPtr<FJsonObject>& Obj, const FString& Key, double Default) const;

	// Blackboard e sensor.
	void SetNum(FName Key, double V) { FNWBBValue& B = BB.FindOrAdd(Key); B.Num = V; B.bVec = false; }
	void SetVec(FName Key, const FVector& V) { FNWBBValue& B = BB.FindOrAdd(Key); B.Vec = V; B.bVec = true; }
	void UpdateSensor(float Dt);
	ANWCharacter* LookForTarget() const;

	// Recargas nomeadas (CooldownDecorator, SetCooldownAction, IsCooldownActiveCondition).
	void StartCooldown(const FNWBTNode& N);
	bool CooldownActive(FName Name) const;

	// Movimento.
	void MoveToward(const FVector& Dest, float Speed, float TurnRate, bool bFaceTarget, float Dt);
	void FaceTarget(float TurnRate, float Dt);

	UFUNCTION() void OnHit(float Damage, ENWHitResult Result, AActor* Attacker);

	const FNWBTAsset* Asset = nullptr;
	FName SetName;
	TMap<FName, FNWBBValue> BB;
	TMap<FName, double> Cooldowns;
	TMap<int32, double> FrequencyReady;
	TMap<int32, int32> PreservedChoice;
	TSharedPtr<FPlan> Current;
	TArray<int32> CurrentSig;
	double LastEval = -100.0;
	TArray<double> WatchedValues;
	TWeakObjectPtr<ANWCharacter> Target;
	/** Próxima procura de alvo (visão): a cada LookEvery s, defasada por criatura, não a cada ciclo. */
	double NextLookAt = 0.0;
	static constexpr double LookEvery = 0.25;
	double TargetLostAt = -1.0;
	FVector HomePos = FVector::ZeroVector;
	TArray<FVector> Path;
	int32 PathIndex = 0;
	bool bResetting = false;
	// Tags do Mannequin que a árvore testa (Alerted, ChainAttack...): marcadas por tarefas e por SetMannequinTag.
	TSet<FName> Tags;
};
