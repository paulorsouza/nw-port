// Mana do jogador (sub-esteira 1a): docs/game/weapons/ice-gauntlet.md §2.
#include "NWManaComponent.h"
#include "Net/UnrealNetwork.h"
#include "NWDevRegistry.h"
#include "NWPlayerController.h"
#include "NWText.h"
#include "GameFramework/Pawn.h"
#include "Misc/ConfigCacheIni.h"

// Mana infinita fica ligada entre sessões de teste (GameUserSettings.ini, [NW.Dev]).
static const TCHAR* DevSection = TEXT("NW.Dev");

UNWManaComponent::UNWManaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UNWManaComponent::BeginPlay()
{
	Super::BeginPlay();
	// Na BeginPlay o jogador ainda não foi possuído: vale para todo personagem (bonecos e mobs não usam mana).
	if (GetOwner()->HasAuthority())
	{
		GConfig->GetBool(DevSection, TEXT("InfiniteMana"), bInfiniteMana, GGameUserSettingsIni);
	}
}

void UNWManaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNWManaComponent, Mana);
	DOREPLIFETIME(UNWManaComponent, bInfiniteMana);
}

void UNWManaComponent::Pay(float Cost)
{
	if (Cost <= 0.f || bInfiniteMana || !GetOwner()->HasAuthority())
	{
		return;
	}
	Mana = FMath::Max(Mana - Cost, 0.f);
}

void UNWManaComponent::Add(float Amount)
{
	if (GetOwner()->HasAuthority())
	{
		Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
	}
}

void UNWManaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner()->HasAuthority() && RegenPerSecond > 0.f && Mana < MaxMana)
	{
		Mana = FMath::Min(Mana + RegenPerSecond * DeltaTime, MaxMana);
	}
}

static UNWManaComponent* PlayerMana(ANWPlayerController* PC)
{
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	return P ? P->FindComponentByClass<UNWManaComponent>() : nullptr;
}

NW_REGISTER_DEV(ManaRefill, TEXT("armas.mana_refill"), TEXT("nw.dev.mana_refill"),
	[](ANWPlayerController* PC)
	{
		if (UNWManaComponent* M = PlayerMana(PC))
		{
			M->Refill();
		}
	});

NW_REGISTER_DEV(ManaInfinite, TEXT("armas.mana_infinite"), TEXT("nw.dev.mana_infinite"),
	[](ANWPlayerController* PC)
	{
		if (UNWManaComponent* M = PlayerMana(PC))
		{
			M->bInfiniteMana = !M->bInfiniteMana;
			M->Refill();
			GConfig->SetBool(DevSection, TEXT("InfiniteMana"), M->bInfiniteMana, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	},
	[](const ANWPlayerController* PC)
	{
		const APawn* P = PC ? PC->GetPawn() : nullptr;
		const UNWManaComponent* M = P ? P->FindComponentByClass<UNWManaComponent>() : nullptr;
		return FText::FromString(NWText::Get(M && M->bInfiniteMana ? TEXT("nw.dev.on") : TEXT("nw.dev.off")));
	});
