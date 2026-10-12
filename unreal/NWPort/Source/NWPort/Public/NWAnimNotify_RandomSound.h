#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "NWAnimNotify_RandomSound.generated.h"

class USoundBase;

// Toca uma das variações ao acaso, como os eventos Random do Wwise do jogo. Gravado pelo import_nw.py.
UCLASS(meta = (DisplayName = "NW Som Aleatório"))
class NWPORT_API UNWAnimNotify_RandomSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") TArray<TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") float Volume = 1.f;
	// Osso de onde o som sai (pé nos passos). Vazio = posição da malha.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") FName Bone;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW") FString Label;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return Label.IsEmpty() ? TEXT("NW Som") : Label; }
};
