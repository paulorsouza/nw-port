#include "NWGatherField.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FNWDepletedNode::PostReplicatedAdd(const FNWDepletedList& List)
{
	if (List.Owner)
	{
		List.Owner->SetDepletedVisual(Index, true);
	}
}

void FNWDepletedNode::PreReplicatedRemove(const FNWDepletedList& List)
{
	if (List.Owner)
	{
		List.Owner->SetDepletedVisual(Index, false);
	}
}

ANWGatherField::ANWGatherField()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(2.f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Depleted.Owner = this;
}

ANWGatherField* ANWGatherField::Get(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ANWGatherField> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

FString ANWGatherField::DataFileFor(const UWorld* World)
{
	// Mapa -> região. Por enquanto só Guardaventos (L_SouthWindsward).
	const FString Map = World ? World->GetMapName() : FString();
	if (Map.Contains(TEXT("SouthWindsward")))
	{
		return FPaths::ProjectDir() / TEXT("Data/World/guardaventos_gatherables.json");
	}
	return FString();
}

void ANWGatherField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWGatherField, Depleted);
}

float ANWGatherField::ServerNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

void ANWGatherField::BeginPlay()
{
	Super::BeginPlay();
	Depleted.Owner = this;
	const FString File = DataFileFor(GetWorld());
	if (File.IsEmpty() || !Load(File))
	{
		UE_LOG(LogTemp, Warning, TEXT("NW: sem coletáveis para %s"), *GetWorld()->GetMapName());
		return;
	}
	// Cliente que entrou depois: o que já veio esgotado na replicação inicial.
	for (const FNWDepletedNode& D : Depleted.Items)
	{
		SetDepletedVisual(D.Index, true);
	}
}

// Onde a instância fica escondida: bem abaixo do terreno (troca de lugar é mais barata que remover/adicionar).
static const FVector HiddenOffset(0.f, 0.f, -200000.f);

bool ANWGatherField::Load(const FString& File)
{
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		return false;
	}
	const bool bRender = !IsRunningDedicatedServer();
	auto MeshFor = [](const FString& Cgf) -> UStaticMesh*
	{
		if (Cgf.IsEmpty())
		{
			return nullptr;
		}
		const FString Name = FPaths::GetBaseFilename(Cgf);
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/NW/Gatherables/%s/%s.%s"), *Name, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	auto MakeComponent = [this](UStaticMesh* Mesh, bool bCollides, float CullDistance, bool bSmall) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(RootComponent);
		C->SetStaticMesh(Mesh);
		C->SetCollisionProfileName(bCollides ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetCanEverAffectNavigation(false);
		// Arbusto/planta/pedrinha somem com a distância (100 mil instâncias não cabem na VRAM de 8 GB todas à vista).
		if (CullDistance > 0.f)
		{
			C->SetCullDistances(int32(CullDistance * 0.8f), int32(CullDistance));
		}
		// Vegetação baixa e pedrinhas fora da cena do Lumen (cartões e campo de distância de dezenas de milhares
		// de instâncias): continuam iluminadas e com sombra, só não entram na iluminação global.
		if (bSmall)
		{
			C->bAffectDistanceFieldLighting = false;
			C->SetAffectDynamicIndirectLighting(false);
			C->bVisibleInRayTracing = false;
		}
		C->RegisterComponent();
		return C;
	};

	const auto& VariantValues = Root->GetObjectField(TEXT("variants"))->Values;
	Variants.SetNum(VariantValues.Num());
	for (const auto& Pair : VariantValues)
	{
		const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
		const int32 Index = O->GetIntegerField(TEXT("index"));
		if (!Variants.IsValidIndex(Index))
		{
			continue;
		}
		FVariant& V = Variants[Index];
		V.Id = FName(*Pair.Key);
		V.Gatherable = FName(O->GetStringField(TEXT("gatherable")));
		V.Mesh = O->GetStringField(TEXT("mesh"));
		V.DepletedMesh = O->GetStringField(TEXT("depleted_mesh"));
		V.Radius = O->GetNumberField(TEXT("interaction_radius"));
		V.NameKey = O->GetStringField(TEXT("name_key")).TrimChar(TEXT('@')).ToLower();
		V.Tradeskill = FName(O->GetStringField(TEXT("tradeskill")));
		V.Action = FName(O->GetStringField(TEXT("action")));
		V.Restriction = FName(O->GetStringField(TEXT("restriction")));
		V.Loot = FName(O->GetStringField(TEXT("loot")));
		V.Time = O->GetNumberField(TEXT("time"));
		double Num = 0.0;
		V.RequiredLevel = O->TryGetNumberField(TEXT("required_level"), Num) ? int32(Num) : 0;
		V.TradeXp = O->TryGetNumberField(TEXT("trade_xp"), Num) ? int32(Num) : 0;
		const TArray<TSharedPtr<FJsonValue>>& R = O->GetArrayField(TEXT("respawn"));
		V.RespawnMin = R.Num() > 0 ? R[0]->AsNumber() : 300.f;
		V.RespawnMax = R.Num() > 1 ? R[1]->AsNumber() : V.RespawnMin;
		// Bloqueiam o caminho: veios, pedregulhos e troncos (arbusto, planta e árvore não; árvore sem tronco simples ainda).
		const FString Kind = O->GetStringField(TEXT("kind"));
		V.bCollides = Kind == TEXT("minerals") || Kind == TEXT("logs");
		// Distância de desenho (cm): árvores e minérios longe; o resto perto, como o jogo faz com vegetação baixa.
		const bool bSmall = Kind != TEXT("trees") && Kind != TEXT("minerals") && Kind != TEXT("logs");
		const float Cull = Kind == TEXT("trees") ? 60000.f : bSmall ? 9000.f : 25000.f;
		if (bRender || V.bCollides)
		{
			if (UStaticMesh* M = MeshFor(V.Mesh))
			{
				V.Full = MakeComponent(M, V.bCollides, Cull, bSmall);
			}
		}
		if (bRender)
		{
			if (UStaticMesh* M = MeshFor(V.DepletedMesh))
			{
				V.Depleted = MakeComponent(M, false, Cull, bSmall);
			}
		}
	}

	// Instâncias: [variante, x, y, z, qx, qy, qz, qw, escala]; junta por variante e adiciona em lote.
	const TArray<TSharedPtr<FJsonValue>>& List = Root->GetArrayField(TEXT("instances"));
	Nodes.Reserve(List.Num());
	TArray<TArray<FTransform>> FullXf, DepletedXf;
	FullXf.SetNum(Variants.Num());
	DepletedXf.SetNum(Variants.Num());
	int32 MissingMeshes = 0;
	for (const TSharedPtr<FJsonValue>& IV : List)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = IV->AsArray();
		if (A.Num() < 9 || !Variants.IsValidIndex(int32(A[0]->AsNumber())))
		{
			continue;
		}
		FNode N;
		N.Variant = int32(A[0]->AsNumber());
		N.Transform = FTransform(FQuat(A[4]->AsNumber(), A[5]->AsNumber(), A[6]->AsNumber(), A[7]->AsNumber()).GetNormalized(),
			FVector(A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber()), FVector(A[8]->AsNumber()));
		const FVariant& V = Variants[N.Variant];
		if (V.Full)
		{
			N.FullInstance = FullXf[N.Variant].Add(N.Transform);
		}
		else
		{
			++MissingMeshes;
		}
		if (V.Depleted)
		{
			FTransform Hidden = N.Transform;
			Hidden.AddToTranslation(HiddenOffset);
			N.DepletedInstance = DepletedXf[N.Variant].Add(Hidden);
		}
		const int32 Index = Nodes.Add(N);
		const FVector L = N.Transform.GetLocation();
		Grid.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / CellSize), FMath::FloorToInt(L.Y / CellSize))).Add(Index);
	}
	for (int32 i = 0; i < Variants.Num(); ++i)
	{
		if (Variants[i].Full && FullXf[i].Num() > 0)
		{
			Variants[i].Full->AddInstances(FullXf[i], false);
		}
		if (Variants[i].Depleted && DepletedXf[i].Num() > 0)
		{
			Variants[i].Depleted->AddInstances(DepletedXf[i], false);
		}
	}
	// No servidor dedicado só os que colidem têm malha; "sem malha" só faz sentido no cliente.
	UE_LOG(LogTemp, Log, TEXT("NW: %d coletáveis de %d variantes (%d sem malha%s) em %s"), Nodes.Num(), Variants.Num(), MissingMeshes,
		bRender ? TEXT("") : TEXT(", servidor: só colisão"), *File);
	return true;
}

bool ANWGatherField::InReach(int32 Index, const FVector& Location, float Slack) const
{
	if (!Nodes.IsValidIndex(Index))
	{
		return false;
	}
	const FVector L = GetActorTransform().TransformPosition(Nodes[Index].Transform.GetLocation());
	return FVector::Dist2D(L, Location) <= VariantOf(Index).Radius + Slack && FMath::Abs(L.Z - Location.Z) < 400.f;
}

int32 ANWGatherField::FindNear(const FVector& Location, const FVector& Facing) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	const FIntPoint C(FMath::FloorToInt(Local.X / CellSize), FMath::FloorToInt(Local.Y / CellSize));
	int32 Best = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 DX = -1; DX <= 1; ++DX)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			const TArray<int32>* Cell = Grid.Find(C + FIntPoint(DX, DY));
			if (!Cell)
			{
				continue;
			}
			for (const int32 I : *Cell)
			{
				if (Nodes[I].bDepleted || !InReach(I, Location))
				{
					continue;
				}
				// Mais perto e mais à frente do personagem primeiro.
				const FVector To = GetActorTransform().TransformPosition(Nodes[I].Transform.GetLocation()) - Location;
				const float Score = To.Size2D() * (1.5f - 0.5f * FVector::DotProduct(To.GetSafeNormal2D(), Facing.GetSafeNormal2D()));
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = I;
				}
			}
		}
	}
	return Best;
}

void ANWGatherField::SetDepletedVisual(int32 Index, bool bIsDepleted)
{
	if (!Nodes.IsValidIndex(Index) || Nodes[Index].bDepleted == bIsDepleted)
	{
		return;
	}
	FNode& N = Nodes[Index];
	N.bDepleted = bIsDepleted;
	const FVariant& V = Variants[N.Variant];
	FTransform Shown = N.Transform, Hidden = N.Transform;
	Hidden.AddToTranslation(HiddenOffset);
	// Esgotado: a cheia some (e a colisão junto), a de fragmentos do jogo aparece no lugar.
	if (V.Full && N.FullInstance != INDEX_NONE)
	{
		V.Full->UpdateInstanceTransform(N.FullInstance, bIsDepleted ? Hidden : Shown, false, true, true);
	}
	if (V.Depleted && N.DepletedInstance != INDEX_NONE)
	{
		V.Depleted->UpdateInstanceTransform(N.DepletedInstance, bIsDepleted ? Shown : Hidden, false, true, true);
	}
}

void ANWGatherField::Deplete(int32 Index)
{
	if (!HasAuthority() || !Nodes.IsValidIndex(Index) || Nodes[Index].bDepleted)
	{
		return;
	}
	const FVariant& V = VariantOf(Index);
	FNWDepletedNode& D = Depleted.Items.AddDefaulted_GetRef();
	D.Index = Index;
	D.RespawnAt = ServerNow() + FMath::FRandRange(V.RespawnMin, FMath::Max(V.RespawnMin, V.RespawnMax));
	Depleted.MarkItemDirty(D);
	SetDepletedVisual(Index, true);
	ForceNetUpdate();
}

void ANWGatherField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	const float Now = ServerNow();
	bool bChanged = false;
	for (int32 i = Depleted.Items.Num() - 1; i >= 0; --i)
	{
		if (Depleted.Items[i].RespawnAt <= Now)
		{
			SetDepletedVisual(Depleted.Items[i].Index, false);
			Depleted.Items.RemoveAt(i);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		Depleted.MarkArrayDirty();
		ForceNetUpdate();
	}
}
