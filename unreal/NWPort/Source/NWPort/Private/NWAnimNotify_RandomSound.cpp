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
	// Weapon impact on the hit reaction: skipped if the attacker was a creature (the bite/gore already sounds in the attack).
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
