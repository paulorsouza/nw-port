#include "NWLootBag.h"

#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWHUD.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWPlayerController.h"
#include "NWText.h"
#include "NWBackend.h"
#include "NWOps.h"
#include "Engine/GameInstance.h"

namespace
{
	// The game's sack, already imported with the region (arc_sacks; Windsward roaddressing). Without it, a small cube.
	const TCHAR* SackMesh = TEXT("/Game/NW/Region/roaddressing/windsward_roaddressing/StaticMeshes/windsward_roaddressing_mesh_20.windsward_roaddressing_mesh_20");
}

ANWLootBag::ANWLootBag()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true;   // only whoever dropped it sees it
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;
	Interact = CreateDefaultSubobject<UNWInteractableComponent>(TEXT("Interact"));
	Interact->ActionKey = TEXT("ui_open");
	Interact->Radius = 250.f;
	Interact->FocusHeight = 40.f;
}

void ANWLootBag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANWLootBag, Items);
}

void ANWLootBag::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Sack = LoadObject<UStaticMesh>(nullptr, SackMesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Mesh->SetStaticMesh(Sack ? Sack : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	if (!Sack)
	{
		Mesh->SetRelativeScale3D(FVector(0.3f));
	}
	if (HasAuthority())
	{
		SetLifeSpan(LifeSeconds);
	}
	TWeakObjectPtr<ANWLootBag> Self(this);
	Interact->OnInteract = [Self](ANWCharacter* Who)
	{
		// Opens the bag screen (client); taking goes through the server (ServerLootTake*).
		ANWPlayerController* PC = Self.IsValid() && Who ? Cast<ANWPlayerController>(Who->GetController()) : nullptr;
		if (ANWHUD* HUD = PC ? Cast<ANWHUD>(PC->GetHUD()) : nullptr)
		{
			HUD->OpenLootBag(Who, Self.Get());
		}
	};
	UpdateLabel();
}

void ANWLootBag::OnRep_Items()
{
	UpdateLabel();
}

void ANWLootBag::UpdateLabel()
{
	// Card: "1 item" / "N items" (ui_loot_bag_*), like the game's container.
	int32 N = 0;
	for (const FNWItem& I : Items)
	{
		N += I.Quantity;
	}
	Interact->DisplayName = FText::FromString(N == 1 ? NWText::Get(TEXT("ui_loot_bag_1_item"))
		: NWText::Get(TEXT("ui_loot_bag_items")).Replace(TEXT("{amount}"), *FString::FromInt(N)));
}

float ANWLootBag::ItemsWeight(const TArray<FNWItem>& InItems)
{
	float W = 0.f;
	for (const FNWItem& I : InItems)
	{
		if (const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId))
		{
			W += D->Weight * I.Quantity;
		}
	}
	return W;
}

float ANWLootBag::GroundWeight(const AController* Owner)
{
	float W = 0.f;
	if (Owner && Owner->GetWorld())
	{
		for (TActorIterator<ANWLootBag> It(Owner->GetWorld()); It; ++It)
		{
			W += It->GetOwner() == Owner && !It->IsActorBeingDestroyed() ? It->Weight() : 0.f;
		}
	}
	return W;
}

bool ANWLootBag::CheckLimit(const ANWCharacter* Who, const TArray<FNWItem>& InItems, FString* OutError)
{
	if (GroundWeight(Who ? Who->GetController() : nullptr) + ItemsWeight(InItems) > MaxGroundWeight)
	{
		if (OutError)
		{
			*OutError = TEXT("nw.inv.ground_full");
		}
		return false;
	}
	return true;
}

FVector ANWLootBag::DropPoint(const ANWCharacter* Who)
{
	// At the feet, a little ahead.
	const float Half = Who->GetSimpleCollisionHalfHeight();
	return Who->GetActorLocation() + Who->GetActorForwardVector() * 60.f - FVector(0.f, 0.f, Half);
}

ANWLootBag* ANWLootBag::Spawn(AController* Owner, const FVector& At, const TArray<FNWItem>& InItems, const FString& InBagId, float Life)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || InItems.Num() == 0)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.Owner = Owner;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANWLootBag* Bag = World->SpawnActor<ANWLootBag>(At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), P);
	if (Bag)
	{
		Bag->BagId = InBagId;
		for (FNWItem I : InItems)
		{
			I.Container = ENWItemContainer::Bag;
			I.Slot = Bag->Items.Num();
			Bag->Items.Add(I);
		}
		Bag->SetLifeSpan(FMath::Max(1.f, Life));
		Bag->UpdateLabel();
		Bag->ForceNetUpdate();
	}
	return Bag;
}

ANWLootBag* ANWLootBag::Drop(ANWCharacter* Who, const TArray<FNWItem>& InItems, bool bLimit, FString* OutError)
{
	AController* PC = Who ? Who->GetController() : nullptr;
	if (!PC || !Who->HasAuthority() || InItems.Num() == 0 || (bLimit && !CheckLimit(Who, InItems, OutError)))
	{
		return nullptr;
	}
	return Spawn(PC, DropPoint(Who), InItems, FString(), LifeSeconds);
}

void ANWLootBag::RestoreFor(ANWPlayerController* PC)
{
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	UNWBackend* B = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UNWBackend>() : nullptr;
	if (!B || !B->IsServerEnabled() || !PC->Record)
	{
		return;
	}
	TWeakObjectPtr<ANWPlayerController> WeakPC(PC);
	B->ListGround(PC->Record->Id, [WeakPC](bool bOk, const TArray<UNWBackend::FGroundBag>& Bags)
	{
		ANWPlayerController* P = WeakPC.Get();
		if (!bOk || !P)
		{
			return;
		}
		const FString Map = UWorld::RemovePIEPrefix(P->GetWorld()->GetMapName());
		for (const UNWBackend::FGroundBag& G : Bags)
		{
			bool bThere = false;
			for (TActorIterator<ANWLootBag> It(P->GetWorld()); It && !bThere; ++It)
			{
				bThere = It->BagId == G.Id;
			}
			if (!bThere && G.Map == Map && G.ExpiresIn > 1.0)
			{
				Spawn(P, G.Position, G.Items, G.Id, float(G.ExpiresIn));
			}
		}
	});
}

void ANWLootBag::RemoveFor(const AController* Owner)
{
	if (!Owner || !Owner->GetWorld())
	{
		return;
	}
	for (TActorIterator<ANWLootBag> It(Owner->GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Owner)
		{
			It->Destroy();
		}
	}
}

void ANWLootBag::Take(ANWCharacter* Who, int32 Slot, int32 Quantity)
{
	if (!Who || !Who->Inventory || !HasAuthority() || Quantity <= 0)
	{
		return;
	}
	// Recorded operation: the bag with what came in and the sack with what is left, in one transaction (NWOps; ground/take).
	TWeakObjectPtr<ANWLootBag> Self(this);
	TSharedRef<TArray<FNWItem>> Left = MakeShared<TArray<FNWItem>>();
	NWOps::Run(Who, TEXT("ground_take"),
		[Self, Who, Slot, Quantity, Left]()
		{
			if (!Self.IsValid())
			{
				return false;
			}
			*Left = Self->Items;
			int32 Moved = 0;
			for (FNWItem& I : *Left)
			{
				if (Slot != INDEX_NONE && I.Slot != Slot)
				{
					continue;
				}
				FNWItem Part = I;
				Part.Quantity = FMath::Min(Quantity, I.Quantity);
				const int32 Got = Part.Quantity - Who->Inventory->AddItem(Part);
				I.Quantity -= Got;
				Moved += Got;
			}
			Left->RemoveAll([](const FNWItem& X) { return X.Quantity <= 0; });
			return Moved > 0;
		},
		[Self, Left](const FNWCharacterRecord& After, const FNWOpState&, TFunction<void(NWOps::EResult)> Fin)
		{
			UNWBackend* B = Self.IsValid() && Self->GetGameInstance() ? Self->GetGameInstance()->GetSubsystem<UNWBackend>() : nullptr;
			if (!B || Self->BagId.IsEmpty())
			{
				Fin(NWOps::EResult::Ok);   // sack only on this server
				return;
			}
			B->TakeGround(After, Self->BagId, *Left, TEXT("ground_take"), [Self, Fin](int32 Status)
			{
				if (Status == 409 && Self.IsValid())
				{
					Self->Destroy();   // expired or already taken
				}
				Fin(Status == 200 || Status == 208 ? NWOps::EResult::Ok : NWOps::EResult::Failed);
			});
		},
		[Self, Left](bool bOk)
		{
			if (!bOk || !Self.IsValid())
			{
				return;
			}
			Self->Items = *Left;
			if (Self->Items.Num() == 0)
			{
				Self->Destroy();
				return;
			}
			Self->UpdateLabel();
			Self->ForceNetUpdate();
		});
}
