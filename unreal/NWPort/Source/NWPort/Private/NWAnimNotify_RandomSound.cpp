#include "NWAnimNotify_RandomSound.h"

#include "NWCombatComponent.h"
#include "NWOptions.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UNWAnimNotify_RandomSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || Sounds.Num() == 0)
	{
		return;
	}
	// Impacto de arma na reação a golpe: não toca se quem bateu foi uma criatura (a mordida/chifrada já soa no ataque).
	if (Label == TEXT("impacto"))
	{
		const UNWCombatComponent* Combat = MeshComp->GetOwner() ? MeshComp->GetOwner()->FindComponentByClass<UNWCombatComponent>() : nullptr;
		if (Combat && Combat->bLastHitByCreature)
		{
			return;
		}
	}
	USoundBase* Sound = Sounds[FMath::RandRange(0, Sounds.Num() - 1)];
	if (!Sound)
	{
		return;
	}
	const FVector At = Bone.IsNone() ? MeshComp->GetComponentLocation() : MeshComp->GetSocketLocation(Bone);
	UGameplayStatics::PlaySoundAtLocation(MeshComp, Sound, At, Volume * FNWOptions::Get().SfxVolume);
}
