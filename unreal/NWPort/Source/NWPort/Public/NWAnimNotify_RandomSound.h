#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "NWAnimNotify_RandomSound.generated.h"

class USoundBase;

// Plays one of the variations at random, like the game's Wwise Random events. Written by import_nw.py.
UCLASS(meta = (DisplayName = "NW Som Aleatório"))
class NWPORT_API UNWAnimNotify_RandomSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") TArray<TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") float Volume = 1.f;
	// Bone the sound comes from (the foot for footsteps). Empty = mesh position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") FName Bone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") FString Label;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return Label.IsEmpty() ? TEXT("NW Som") : Label; }
};
