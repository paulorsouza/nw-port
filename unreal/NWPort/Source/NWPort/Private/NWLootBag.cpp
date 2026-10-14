#include "NWLootBag.h"

#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWText.h"

namespace
{
	// Saco do jogo já importado com a região (arc_sacks; roaddressing de Guardaventos). Sem ele, um cubo pequeno.
	const TCHAR* SackMesh = TEXT("/Game/NW/Region/roaddressing/windsward_roaddressing/StaticMeshes/windsward_roaddressing_mesh_20.windsward_roaddressing_mesh_20");
}

ANWLootBag::ANWLootBag()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true;   // só quem soltou vê
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;
	Interact = CreateDefaultSubobject<UNWInteractableComponent>(TEXT("Interact"));
	Interact->ActionKey = TEXT("ui_takeall");
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
		if (Self.IsValid() && Who && Who->Interactor)
		{
			Who->Interactor->ServerLootTake(Self.Get());
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
	// Cartão: "1 item" / "N itens" (ui_loot_bag_*), como o contêiner do jogo.
	int32 N = 0;
	for (const FNWItem& I : Items)
	{
		N += I.Quantity;
	}
	Interact->DisplayName = FText::FromString(N == 1 ? NWText::Get(TEXT("ui_loot_bag_1_item"))
		: NWText::Get(TEXT("ui_loot_bag_items")).Replace(TEXT("{amount}"), *FString::FromInt(N)));
}

float ANWLootBag::Weight() const
{
	float W = 0.f;
	for (const FNWItem& I : Items)
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

ANWLootBag* ANWLootBag::Drop(ANWCharacter* Who, const TArray<FNWItem>& InItems, bool bLimit, FString* OutError)
{
	AController* PC = Who ? Who->GetController() : nullptr;
	if (!PC || !Who->HasAuthority() || InItems.Num() == 0)
	{
		return nullptr;
	}
	float Add = 0.f;
	for (const FNWItem& I : InItems)
	{
		const FNWItemDef* D = FNWItemCatalog::Get().Find(I.ItemId);
		Add += D ? D->Weight * I.Quantity : 0.f;
	}
	if (bLimit && GroundWeight(PC) + Add > MaxGroundWeight)
	{
		if (OutError)
		{
			*OutError = TEXT("nw.inv.ground_full");
		}
		return nullptr;
	}
	// Aos pés, um pouco à frente, girado ao acaso.
	const float Half = Who->GetSimpleCollisionHalfHeight();
	const FVector At = Who->GetActorLocation() + Who->GetActorForwardVector() * 60.f - FVector(0.f, 0.f, Half);
	FActorSpawnParameters P;
	P.Owner = PC;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANWLootBag* Bag = Who->GetWorld()->SpawnActor<ANWLootBag>(At, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), P);
	if (Bag)
	{
		for (FNWItem I : InItems)
		{
			I.Container = ENWItemContainer::Bag;
			I.Slot = Bag->Items.Num();
			Bag->Items.Add(I);
		}
		Bag->UpdateLabel();
		Bag->ForceNetUpdate();
	}
	return Bag;
}

void ANWLootBag::TakeAll(ANWCharacter* Who)
{
	if (!Who || !Who->Inventory || !HasAuthority())
	{
		return;
	}
	TArray<FNWItem> Left;
	for (const FNWItem& I : Items)
	{
		const int32 Rest = Who->Inventory->AddItem(I);
		if (Rest > 0)
		{
			FNWItem K = I;
			K.Quantity = Rest;
			Left.Add(K);
		}
	}
	Items = MoveTemp(Left);
	if (Items.Num() == 0)
	{
		Destroy();
		return;
	}
	UpdateLabel();
	ForceNetUpdate();
}
