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
	// Capsule the size of the player's: blocks whoever walks and serves as a target for the interaction focus.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 86.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = Capsule;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	// The game mesh faces +Y: same correction as the player (NWCharacter), so the actor and the mesh face together
	// (turning to whoever talks).
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -86.f), FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Interact = CreateDefaultSubobject<UNWInteractableComponent>(TEXT("Interact"));
	Interact->FocusHeight = 0.f;   // capsule center (the card rises 90 cm from there)
}

void ANWNpc::Setup(const FString& InVariant, const FText& InName, const FString& Mesh, const FString& Anim)
{
	Variant = InVariant;
	DisplayName = InName;
	// Ambient NPC (no name in the game) has no conversation card.
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
		UE_LOG(LogTemp, Warning, TEXT("NPC %s: no mesh %s"), *Variant, *Mesh);
	}
	if (UAnimSequence* A = Anim.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Anim))
	{
		Body->PlayAnimation(A, true);
		// Different start point per NPC: several with the same animation do not move in sync.
		Body->SetPosition(FMath::Fmod(float(GetTypeHash(Variant) % 1000) * 0.037f, A->GetPlayLength()), false);
	}
	else if (!Anim.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC %s: no animation %s"), *Variant, *Anim);
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
		UE_LOG(LogTemp, Warning, TEXT("NPCs: could not read %s"), *File);
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
		// pos is the foot (capital point). Snaps to the converted ground just below or above (up to 1.5 m) and puts the
		// capsule center 86 cm above it.
		FVector Foot(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NWNpcFloor), true);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0, 0, 150), Foot - FVector(0, 0, 150), ECC_Visibility, Q))
		{
			Foot = Hit.ImpactPoint;
		}
		const FVector Loc = Foot + FVector(0, 0, 86.0);
		// The data "yaw" is the actor's with the uncorrected mesh; +90 compensates the mesh rotation above (same pose).
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
		// Port shop merchant (tools/shops.json): card "Loja · [E] Comprar" opens the shop screen on the client.
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
	UE_LOG(LogTemp, Log, TEXT("NPCs: %d from %s"), Npcs.Num(), *File);
	// Training field of the weapon tutorials: dummies (character without AI, respawns after death). Server only:
	// the dummy is replicated.
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

	// City refining stations (Data/Crafting/refining.json; the mesh is the converted city one). Only this map's city:
	// each station position is in its own city's map frame.
	auto OfThisCity = [this](const FNWCrafting::FStation& Data) { return Data.City.IsEmpty() || Data.City == City; };
	for (const FNWCrafting::FStation& Data : FNWCrafting::Get().Stations())
	{
		if (!OfThisCity(Data))
		{
			continue;
		}
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

	// City area: center of the NPCs and stations, radius to the farthest + CityMargin. Only the server marks it.
	TArray<FVector> Points;
	for (const ANWNpc* Npc : Npcs)
	{
		Points.Add(Npc->GetActorLocation());
	}
	for (const FNWCrafting::FStation& Data : FNWCrafting::Get().Stations())
	{
		if (OfThisCity(Data))
		{
			Points.Add(Data.Pos);
		}
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
	// With nw-core, only after the character has loaded from it (otherwise the Load would erase the marked city).
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
				GM->SavePlayer(PC, TEXT("city:") + City);   // city and capacity in nw-core right away
			}
		}
	}
}
