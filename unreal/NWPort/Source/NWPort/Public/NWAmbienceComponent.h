#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWAmbienceComponent.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Background sound of the local player in Windsward: territory music (outside the city) or city music, and under it the
 * game ambience (wildlife / settlement movement). Crossfades when the character enters or leaves the
 * city. Sounds imported by tools/ue/import_audio.py into /Game/NW/Audio (loop).
 */
UCLASS(ClassGroup = (NW))
class NWPORT_API UNWAmbienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWAmbienceComponent();

	UPROPERTY(EditAnywhere, Category = "NW|Som") TSoftObjectPtr<USoundBase> TerritoryMusic;
	UPROPERTY(EditAnywhere, Category = "NW|Som") TSoftObjectPtr<USoundBase> SettlementMusic;
	UPROPERTY(EditAnywhere, Category = "NW|Som") TSoftObjectPtr<USoundBase> WildlifeAmbience;
	UPROPERTY(EditAnywhere, Category = "NW|Som") TSoftObjectPtr<USoundBase> SettlementAmbience;
	// City radius in the map frame (the city is at the origin of L_SouthWindsward), in cm.
	UPROPERTY(EditAnywhere, Category = "NW|Som") float SettlementRadius = 22000.f;
	UPROPERTY(EditAnywhere, Category = "NW|Som") float MusicVolume = 0.45f;
	UPROPERTY(EditAnywhere, Category = "NW|Som") float AmbienceVolume = 0.8f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Play(TObjectPtr<UAudioComponent>& Slot, USoundBase* Sound, float Volume);

	UPROPERTY() TObjectPtr<UAudioComponent> Music;
	UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
	int32 Zone = -1;  // 0 territory, 1 city
};
