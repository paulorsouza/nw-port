#include "NWAppearance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// Data/Items/appearances.json (tools/make_appearances.py): base body parts, armor piece per item, weapon mesh per item.
	struct FArmorLook
	{
		FString Mesh;
		TArray<FName> Covers;   // base parts the piece replaces (shirt1, gloves1, pants1, boots1, head)
	};
	struct FLookData
	{
		bool bLoaded = false;
		TArray<TPair<FName, FString>> Body;   // in file order: head, shirt1, gloves1, pants1, boots1
		TMap<FName, FArmorLook> Armor;
		TMap<FName, FString> Weapons;
	};

	const FLookData& Data()
	{
		static FLookData D;
		if (D.bLoaded)
		{
			return D;
		}
		D.bLoaded = true;
		FString Raw;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Raw, *NWText::DataPath(TEXT("Data/Items/appearances.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
		{
			return D;
		}
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Root->TryGetObjectField(TEXT("body"), Obj))
		{
			for (const auto& P : (*Obj)->Values)
			{
				D.Body.Add({FName(*P.Key), P.Value->AsString()});
			}
		}
		if (Root->TryGetObjectField(TEXT("armor"), Obj))
		{
			for (const auto& P : (*Obj)->Values)
			{
				const TSharedPtr<FJsonObject> A = P.Value->AsObject();
				FArmorLook L;
				L.Mesh = A->GetStringField(TEXT("mesh"));
				const TArray<TSharedPtr<FJsonValue>>* Covers = nullptr;
				if (A->TryGetArrayField(TEXT("covers"), Covers))
				{
					for (const TSharedPtr<FJsonValue>& C : *Covers)
					{
						L.Covers.Add(FName(C->AsString()));
					}
				}
				D.Armor.Add(FName(*P.Key), L);
			}
		}
		if (Root->TryGetObjectField(TEXT("weapons"), Obj))
		{
			for (const auto& P : (*Obj)->Values)
			{
				D.Weapons.Add(FName(*P.Key), P.Value->AsObject()->GetStringField(TEXT("mesh")));
			}
		}
		return D;
	}

	template <typename T>
	T* LoadMesh(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UStaticMesh* WeaponMesh(FName ItemId)
	{
		const FString* Path = ItemId.IsNone() ? nullptr : Data().Weapons.Find(ItemId);
		return Path ? LoadMesh<UStaticMesh>(*Path) : nullptr;
	}

	FName SetOf(FName ItemId)
	{
		const FNWItemDef* D = ItemId.IsNone() ? nullptr : FNWItemCatalog::Get().Find(ItemId);
		return D ? D->Set : NAME_None;
	}
}

UNWAppearanceComponent::UNWAppearanceComponent()
{
	SetIsReplicatedByDefault(true);
}

void UNWAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNWAppearanceComponent, Look);
}

void UNWAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Look.bFromInventory)
	{
		OnRep_Look();
	}
}

void UNWAppearanceComponent::RefreshFrom(const UNWInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return;
	}
	auto At = [Inventory](int32 Slot)
	{
		const FNWItem* I = Inventory->At(ENWItemContainer::Equip, Slot);
		return I ? I->ItemId : NAME_None;
	};
	FNWLook New;
	New.Head = At(NWEquip::Head);
	New.Chest = At(NWEquip::Chest);
	New.Hands = At(NWEquip::Hands);
	New.Legs = At(NWEquip::Legs);
	New.Feet = At(NWEquip::Feet);
	New.Weapon1 = At(NWEquip::Weapon1);
	New.Weapon2 = At(NWEquip::Weapon2);
	New.Shield = At(NWEquip::Shield);
	New.bFromInventory = true;
	if (New == Look)
	{
		return;
	}
	Look = New;
	GetOwner()->ForceNetUpdate();
	OnRep_Look();   // the server's own view (listen server, editor Play)
}

void UNWAppearanceComponent::OnRep_Look()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;   // nothing to see; the combat reach does not depend on the meshes
	}
	ApplyBody();
	ApplyWeaponLook();
}

USkeletalMeshComponent* UNWAppearanceComponent::Part(FName Name)
{
	if (TObjectPtr<USkeletalMeshComponent>* Found = Parts.Find(Name); Found && *Found)
	{
		return *Found;
	}
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Leader = Owner ? Owner->GetMesh() : nullptr;
	if (!Leader)
	{
		return nullptr;
	}
	USkeletalMeshComponent* P = NewObject<USkeletalMeshComponent>(Owner, Name);
	P->SetupAttachment(Leader);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetReceivesDecals(false);
	P->RegisterComponent();
	P->SetLeaderPoseComponent(Leader);
	Parts.Add(Name, P);
	return P;
}

void UNWAppearanceComponent::ApplyBody()
{
	const FLookData& D = Data();
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Leader = Owner ? Owner->GetMesh() : nullptr;
	if (!Leader || !Look.bFromInventory || D.Body.Num() == 0)
	{
		return;
	}
	// Body parts imported? Without them, keep the old single mesh (heroicheavy).
	if (!LoadMesh<USkeletalMesh>(D.Body[0].Value))
	{
		return;
	}
	const TPair<FName, FName> Slots[] = {{TEXT("Head"), Look.Head}, {TEXT("Chest"), Look.Chest}, {TEXT("Hands"), Look.Hands},
		{TEXT("Legs"), Look.Legs}, {TEXT("Feet"), Look.Feet}};
	TSet<FName> Covered;
	for (const TPair<FName, FName>& S : Slots)
	{
		const FArmorLook* A = S.Value.IsNone() ? nullptr : D.Armor.Find(S.Value);
		USkeletalMesh* Mesh = A ? LoadMesh<USkeletalMesh>(A->Mesh) : nullptr;
		// The slot's component only once a piece was equipped there (afterwards it is kept, hidden when empty).
		const FName Name(*(TEXT("Look_") + S.Key.ToString()));
		USkeletalMeshComponent* P = Mesh || Parts.Contains(Name) ? Part(Name) : nullptr;
		if (P)
		{
			P->SetSkeletalMesh(Mesh);
			P->SetVisibility(Mesh != nullptr);
		}
		if (Mesh)
		{
			Covered.Append(A->Covers);
		}
	}
	for (const TPair<FName, FString>& B : D.Body)
	{
		if (USkeletalMeshComponent* P = Part(FName(*(TEXT("Body_") + B.Key.ToString()))))
		{
			if (!P->GetSkeletalMeshAsset())
			{
				P->SetSkeletalMesh(LoadMesh<USkeletalMesh>(B.Value));
			}
			P->SetVisibility(!Covered.Contains(B.Key));
		}
	}
	// The leader keeps animating (bones for the parts and the weapon sockets) but is not drawn.
	Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Leader->SetVisibility(false, false);
}

void UNWAppearanceComponent::ApplyWeaponLook()
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || !Look.bFromInventory || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FName Active = Me->WeaponSets.IsValidIndex(Me->ActiveWeapon) ? Me->WeaponSets[Me->ActiveWeapon].Id : NAME_None;
	const int32 OtherIndex = Me->OtherWeapon();
	const FName Other = Me->WeaponSets.IsValidIndex(OtherIndex) ? Me->WeaponSets[OtherIndex].Id : NAME_None;
	// The item of each set in Weapon 1/2 (the first one when both have the same set).
	const FName InHand = SetOf(Look.Weapon1) == Active ? Look.Weapon1 : SetOf(Look.Weapon2) == Active ? Look.Weapon2 : NAME_None;
	const FName OnBack = SetOf(Look.Weapon1) == Other && Look.Weapon1 != InHand ? Look.Weapon1
		: SetOf(Look.Weapon2) == Other ? Look.Weapon2 : NAME_None;
	if (UStaticMesh* Mesh = WeaponMesh(InHand); Mesh && Me->Sword)
	{
		Me->Sword->SetStaticMesh(Mesh);
	}
	if (UStaticMesh* Mesh = WeaponMesh(OnBack); Mesh && Me->Stowed)
	{
		Me->Stowed->SetStaticMesh(Mesh);
	}
	if (Me->Shield)
	{
		if (Look.Shield.IsNone())
		{
			Me->Shield->SetVisibility(false);   // no shield equipped: none on the arm or on the back
		}
		else if (UStaticMesh* Mesh = WeaponMesh(Look.Shield))
		{
			Me->Shield->SetStaticMesh(Mesh);
		}
	}
}
