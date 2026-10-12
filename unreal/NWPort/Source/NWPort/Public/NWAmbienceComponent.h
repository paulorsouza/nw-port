#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWAmbienceComponent.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Som de fundo do jogador local em Guardaventos: música do território (fora da cidade) ou da cidade, e por baixo o
 * ambiente do jogo (vida selvagem / movimento do assentamento). Troca com fade quando o personagem entra ou sai da
 * cidade. Sons importados por tools/ue/import_audio.py em /Game/NW/Audio (loop).
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
	// Raio da cidade no referencial do mapa (a cidade fica na origem do L_SouthWindsward), em cm.
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
	int32 Zone = -1;  // 0 território, 1 cidade
};
