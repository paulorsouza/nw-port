#include "NWAmbienceComponent.h"

#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NWGatherField.h"
#include "Sound/SoundBase.h"

namespace
{
	TSoftObjectPtr<USoundBase> AudioAsset(const TCHAR* Path)
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/NW/Audio/%s.%s"), Path, *Name)));
	}
}

UNWAmbienceComponent::UNWAmbienceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.f;
	TerritoryMusic = AudioAsset(TEXT("Music/S_windsward_ambient"));
	SettlementMusic = AudioAsset(TEXT("Music/S_windsward_settlement_august"));
	WildlifeAmbience = AudioAsset(TEXT("Ambient/S_wildlife_woodedhills_day"));
	SettlementAmbience = AudioAsset(TEXT("Ambient/S_pois_settlement_day"));
}

void UNWAmbienceComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	// Só no jogador local, e só em mapa de região (o mesmo critério dos coletáveis).
	if (!PC || !PC->IsLocalController() || IsRunningDedicatedServer() || ANWGatherField::DataFileFor(GetWorld()).IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void UNWAmbienceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (UAudioComponent* C : {Music.Get(), Ambience.Get()})
	{
		if (C)
		{
			C->Stop();
		}
	}
	Super::EndPlay(Reason);
}

void UNWAmbienceComponent::Play(TObjectPtr<UAudioComponent>& Slot, USoundBase* Sound, float Volume)
{
	if (Slot)
	{
		Slot->FadeOut(3.f, 0.f);
		Slot = nullptr;
	}
	if (!Sound)
	{
		return;
	}
	Slot = UGameplayStatics::CreateSound2D(this, Sound, Volume, 1.f, 0.f, nullptr, false, false);
	if (Slot)
	{
		Slot->FadeIn(3.f, Volume);
	}
}

void UNWAmbienceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	const int32 Now = Pawn->GetActorLocation().Size2D() < SettlementRadius ? 1 : 0;
	if (Now == Zone)
	{
		return;
	}
	Zone = Now;
	Play(Music, (Zone ? SettlementMusic : TerritoryMusic).LoadSynchronous(), MusicVolume);
	Play(Ambience, (Zone ? SettlementAmbience : WildlifeAmbience).LoadSynchronous(), AmbienceVolume);
}
