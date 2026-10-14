#include "NWNpc.h"

#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWBackend.h"
#include "NWGameMode.h"
#include "NWItemRules.h"
#include "NWProgressionComponent.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "NWHUD.h"
#include "NWInteractable.h"
#include "NWPlayerController.h"
#include "NWShop.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ANWNpc::ANWNpc()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	// Cápsula do tamanho da do jogador: bloqueia quem anda e serve de alvo para o foco da interação.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 86.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Capsule;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	// Malha do jogo olha para +Y: mesma correção do jogador (NWCharacter), para o ator e a malha olharem juntos (virar
	// para quem conversa).
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -86.f), FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Interact = CreateDefaultSubobject<UNWInteractableComponent>(TEXT("Interact"));
	Interact->FocusHeight = 0.f;   // centro da cápsula (o cartão sobe 90 cm a partir daí)
}

void ANWNpc::Setup(const FString& InVariant, const FText& InName, const FString& Mesh, const FString& Anim)
{
	Variant = InVariant;
	DisplayName = InName;
	// NPC ambiente (sem nome no jogo) não tem cartão de conversa.
	Interact->SetActive(!InName.IsEmpty());
	Interact->DisplayName = InName;
	TWeakObjectPtr<ANWNpc> Self(this);
	Interact->OnInteract = [Self](ANWCharacter* Who)
	{
		if (Self.IsValid() && Who)
		{
			const FVector To = Who->GetActorLocation() - Self->GetActorLocation();
			Self->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
			if (Who->Interactor)
			{
				Who->Interactor->ServerNpcInteract(Self->Variant);
			}
		}
	};
	if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, *Mesh))
	{
		Body->SetSkeletalMesh(M);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC %s: sem malha %s"), *Variant, *Mesh);
	}
	if (UAnimSequence* A = Anim.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Anim))
	{
		Body->PlayAnimation(A, true);
		// Começo em ponto diferente por NPC: vários com a mesma animação não se mexem juntos.
		Body->SetPosition(FMath::Fmod(float(GetTypeHash(Variant) % 1000) * 0.037f, A->GetPlayLength()), false);
	}
	else if (!Anim.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC %s: sem animação %s"), *Variant, *Anim);
	}
}

ANWNpcField::ANWNpcField()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANWNpcField::BeginPlay()
{
	Super::BeginPlay();
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/World") / DataFile;
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("NPCs: não li %s"), *File);
		return;
	}
	Root->TryGetStringField(TEXT("city"), City);
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("npcs")))
	{
		const TSharedPtr<FJsonObject>& N = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& P = N->GetArrayField(TEXT("pos"));
		if (P.Num() != 3)
		{
			continue;
		}
		// pos é o pé (ponto do capital). Encaixa no chão convertido logo abaixo ou acima (até 1,5 m) e põe o centro
		// da cápsula 86 cm acima dele.
		FVector Foot(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NWNpcFloor), true);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 150), Foot - FVector(0, 0, 150), ECC_Visibility, Q))
		{
			Foot = Hit.ImpactPoint;
		}
		const FVector Loc = Foot + FVector(0, 0, 86.0);
		// "yaw" dos dados é o do ator com a malha sem correção; +90 compensa o giro da malha acima (pose igual).
		const FRotator Rot(0.0, N->GetNumberField(TEXT("yaw")) + 90.0, 0.0);
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANWNpc* Npc = GetWorld()->SpawnActor<ANWNpc>(ANWNpc::StaticClass(), Loc, Rot, SP);
		if (!Npc)
		{
			continue;
		}
		const FString MeshName = N->GetStringField(TEXT("mesh"));
		const FString AnimName = N->GetStringField(TEXT("anim"));
		const TSharedPtr<FJsonObject>* Names = nullptr;
		FString Name;
		if (N->TryGetObjectField(TEXT("name"), Names))
		{
			Name = (*Names)->GetStringField(NWText::Lang());
		}
		N->TryGetStringField(TEXT("tipo"), Npc->Type);
		Npc->City = City;
		Npc->Setup(N->GetStringField(TEXT("variant")), FText::FromString(Name),
			FString::Printf(TEXT("%s/NPCs/%s/%s.%s"), *AssetRoot, *MeshName, *MeshName, *MeshName),
			AnimName.IsEmpty() ? FString() : FString::Printf(TEXT("%s/Anims/A_%s.A_%s"), *AssetRoot, *AnimName, *AnimName));
		// Mercador da loja do port (tools/shops.json): cartão "Loja · [E] Comprar" abre a tela da loja no cliente.
		if (const FNWShop* Shop = FNWShops::Get().ForNpc(Npc->Variant))
		{
			const FString ShopId = Shop->Id;
			const FString Title = NWText::Get(TEXT("ui_shop_default_name"));
			Npc->Interact->SetActive(true);
			Npc->Interact->DisplayName = FText::FromString(Title);
			Npc->Interact->ActionKey = TEXT("ui_buy");
			Npc->Interact->OnInteract = [ShopId, Title](ANWCharacter* Who)
			{
				ANWPlayerController* PC = Who ? Cast<ANWPlayerController>(Who->GetController()) : nullptr;
				if (ANWHUD* HUD = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr)
				{
					HUD->OpenShop(Who, ShopId, Title);
				}
			};
		}
		Npcs.Add(Npc);
	}
	UE_LOG(LogTemp, Log, TEXT("NPCs: %d de %s"), Npcs.Num(), *File);
	// Campo de treino dos tutoriais de arma: bonecos (personagem sem IA, renasce depois de morto). Só no servidor:
	// o boneco é replicado.
	const TArray<TSharedPtr<FJsonValue>>* Dummies = nullptr;
	if (HasAuthority() && Root->TryGetArrayField(TEXT("dummies"), Dummies))
	{
		for (const TSharedPtr<FJsonValue>& V : *Dummies)
		{
			const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
			const FVector Foot(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
			const FTransform Where(FRotator(0.f, 180.f, 0.f), Foot + FVector(0.f, 0.f, 100.f));
			if (ANWCharacter* D = GetWorld()->SpawnActorDeferred<ANWCharacter>(ANWCharacter::StaticClass(), Where, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
			{
				D->FinishSpawning(Where);
			}
		}
	}

	// Estações de refino da cidade (Data/Crafting/refining.json; a malha é a da cidade convertida).
	for (const FNWCrafting::FStation& Data : FNWCrafting::Get().Stations())
	{
		FVector Foot = Data.Pos;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NWStationFloor), true);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 150), Foot - FVector(0, 0, 300), ECC_Visibility, Q))
		{
			Foot = Hit.ImpactPoint;
		}
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ANWStation* St = GetWorld()->SpawnActor<ANWStation>(ANWStation::StaticClass(), Foot, FRotator(0.f, Data.Yaw, 0.f), SP))
		{
			St->City = City;
			St->Setup(Data);
		}
	}

	// Área da cidade: centro dos NPCs e estações, raio até o mais longe + CityMargin. Só o servidor marca.
	TArray<FVector> Points;
	for (const ANWNpc* Npc : Npcs)
	{
		Points.Add(Npc->GetActorLocation());
	}
	for (const FNWCrafting::FStation& Data : FNWCrafting::Get().Stations())
	{
		Points.Add(Data.Pos);
	}
	if (HasAuthority() && !City.IsEmpty() && Points.Num() > 0)
	{
		for (const FVector& P : Points)
		{
			CityCenter += P / float(Points.Num());
		}
		for (const FVector& P : Points)
		{
			CityRadius = FMath::Max(CityRadius, float(FVector::Dist2D(P, CityCenter)));
		}
		CityRadius += CityMargin;
		GetWorldTimerManager().SetTimer(DiscoverTimer, this, &ANWNpcField::CheckDiscover, 1.f, true);
	}
}

void ANWNpcField::CheckDiscover()
{
	// Com nw-core, só depois que o personagem carregou dele (senão o Load apagaria a cidade marcada).
	const UGameInstance* GI = GetGameInstance();
	const UNWBackend* B = GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
	const bool bBackend = B && B->IsServerEnabled();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ANWPlayerController* PC = Cast<ANWPlayerController>(It->Get());
		ANWCharacter* Me = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (!Me || !Me->Progression || (bBackend && !PC->Record) || FVector::Dist2D(Me->GetActorLocation(), CityCenter) > CityRadius)
		{
			continue;
		}
		if (Me->Progression->DiscoverCity(City))
		{
			PC->Warning(NWText::Key(TEXT("nw.storage.city_known"), {FString::FromInt(FMath::RoundToInt(FNWItemRules::Get().StoragePerCity))}));
			if (ANWGameMode* GM = GetWorld()->GetAuthGameMode<ANWGameMode>())
			{
				GM->SavePlayer(PC, TEXT("city:") + City);   // cidade e capacidade no nw-core na hora
			}
		}
	}
}
