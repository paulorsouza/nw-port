#include "NWGatherComponent.h"
#include "NWOps.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWGatherField.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "NWCreature.h"
#include "NWText.h"

static const TCHAR* GatherAnims = TEXT("/Game/NW/sword_shield/Anims/");
static const TCHAR* GatherMeshes = TEXT("/Game/NW/sword_shield/Mesh/");

UNWGatherComponent::UNWGatherComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	auto Anim = [](const TCHAR* Name) { return TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), GatherAnims, Name, Name))); };
	auto Mesh = [](const TCHAR* Name) { return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), GatherMeshes, Name, Name))); };
	Montages.Add(TEXT("Mining"), Anim(TEXT("AM_GatherMine")));
	Montages.Add(TEXT("Cutting"), Anim(TEXT("AM_GatherCut")));
	Montages.Add(TEXT("Chopping"), Anim(TEXT("AM_GatherChop")));
	Montages.Add(TEXT("Dressing"), Anim(TEXT("AM_GatherSkin")));
	Montages.Add(TEXT("None"), Anim(TEXT("AM_GatherHand")));
	ToolMeshes.Add(TEXT("Mining"), Mesh(TEXT("SM_pick")));
	ToolMeshes.Add(TEXT("Cutting"), Mesh(TEXT("SM_sickle")));
	ToolMeshes.Add(TEXT("Chopping"), Mesh(TEXT("SM_loggingaxe")));
	ToolMeshes.Add(TEXT("Dressing"), Mesh(TEXT("SM_knife")));
}

void UNWGatherComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNWGatherComponent, GatherIndex);
	DOREPLIFETIME(UNWGatherComponent, GatherAction);
	DOREPLIFETIME(UNWGatherComponent, GatherStart);
	DOREPLIFETIME(UNWGatherComponent, GatherEnd);
	DOREPLIFETIME(UNWGatherComponent, GatherActor);
}

float UNWGatherComponent::ServerNow() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

float UNWGatherComponent::Progress() const
{
	if (!IsGathering() || GatherEnd <= GatherStart)
	{
		return 0.f;
	}
	return FMath::Clamp((ServerNow() - GatherStart) / (GatherEnd - GatherStart), 0.f, 1.f);
}

float UNWGatherComponent::Remaining() const
{
	return IsGathering() ? FMath::Max(0.f, GatherEnd - ServerNow()) : 0.f;
}

FName UNWGatherComponent::ToolType(FName Action)
{
	// Ação do coletável (gatherables.GatheringAction) = GatheringTypes da ferramenta, menos "None" (à mão).
	return Action == TEXT("None") ? NAME_None : Action;
}

FString UNWGatherComponent::ToolGroupKey(FName Action)
{
	if (Action == TEXT("Mining")) return TEXT("2hpick_groupname");
	if (Action == TEXT("Cutting")) return TEXT("sickle_groupname");
	if (Action == TEXT("Chopping")) return TEXT("2haxe_groupname");
	if (Action == TEXT("Dressing")) return TEXT("1hskinningknife_groupname");
	return FString();
}

bool UNWGatherComponent::GetFocusLocation(FVector& Out) const
{
	if (IsGathering() && GatherIndex == SkinIndex)
	{
		if (GatherActor)
		{
			Out = GatherActor->GetActorLocation();
			return true;
		}
		return false;
	}
	if (!IsGathering() && FocusCreature.IsValid())
	{
		Out = FocusCreature->GetActorLocation();
		return true;
	}
	const int32 Index = IsGathering() ? GatherIndex : FocusIndex;
	const ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	if (!Field || Index < 0 || Index >= Field->Num())
	{
		return false;
	}
	Out = Field->GetActorTransform().TransformPosition(Field->Node(Index).Transform.GetLocation());
	return true;
}

void UNWGatherComponent::UpdateFocus()
{
	FocusIndex = INDEX_NONE;
	FocusCreature.Reset();
	FocusText.Reset();
	FocusName.Reset();
	bFocusBlocked = false;
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me || IsGathering() || Me->Combat->State == ENWCombatState::Dead)
	{
		return;
	}
	// Carcaça perto (criatura morta, ainda não esfolada): "Esfolar Javali"; pede a faca de esfolar.
	for (TActorIterator<ANWCreature> It(GetWorld()); It; ++It)
	{
		const FNWCreatureDef* D = It->Def();
		if (It->Combat->State != ENWCombatState::Dead || It->bSkinned || !D || D->SkinLoot.IsNone()
			|| FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) > 250.f)
		{
			continue;
		}
		FocusCreature = *It;
		const FString Name = NWText::Get(D->SkinNameKey.IsEmpty() ? D->NameKey : D->SkinNameKey);
		FocusName = Name;
		if (!(Me->Inventory && Me->Inventory->ToolFor(TEXT("Dressing"))))
		{
			bFocusBlocked = true;
			FocusText = FString::Printf(TEXT("%s  ·  %s"), *Name, *NWText::Get(TEXT("ui_notoolitem")).Replace(TEXT("{itemName}"), *NWText::Get(ToolGroupKey(TEXT("Dressing")))));
			return;
		}
		FocusText = FString::Printf(TEXT("%s %s"), *NWText::Get(TEXT("nw.gather.skin")), *Name);
		return;
	}
	const ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	if (!Field)
	{
		return;
	}
	FocusIndex = Field->FindNear(Me->GetActorLocation(), Me->GetActorForwardVector());
	if (FocusIndex == INDEX_NONE)
	{
		return;
	}
	const ANWGatherField::FVariant& V = Field->VariantOf(FocusIndex);
	const FString Name = NWText::Get(V.NameKey);
	FocusName = Name;
	const FName Tool = ToolType(V.Action);
	if (V.Restriction == TEXT("ToolRequired") && !Tool.IsNone() && !(Me->Inventory && Me->Inventory->ToolFor(Tool)))
	{
		bFocusBlocked = true;
		FocusText = FString::Printf(TEXT("%s  ·  %s"), *Name, *NWText::Get(TEXT("ui_notoolitem")).Replace(TEXT("{itemName}"), *NWText::Get(ToolGroupKey(V.Action))));
		return;
	}
	// Nível mínimo do ofício (RequiredTradeskillLevel): "Requer o Nível 50" com o nome do ofício.
	if (const int32 Need = Me->Progression ? Me->Progression->MissingTradeLevel(V.Tradeskill, V.RequiredLevel) : 0)
	{
		bFocusBlocked = true;
		FocusText = FString::Printf(TEXT("%s  ·  %s (%s)"), *Name,
			*NWText::Get(TEXT("ui_interact_required_level")).Replace(TEXT("{level}"), *FString::FromInt(Need)),
			*NWText::Get(NWTradeskill::TextKey(NWTradeskill::Index(V.Tradeskill))));
		return;
	}
	const TCHAR* Verb = V.Action == TEXT("Mining") ? TEXT("ui_mine") : V.Action == TEXT("Chopping") ? TEXT("ui_log") : TEXT("ui_harvest");
	FocusText = FString::Printf(TEXT("%s %s"), *NWText::Get(Verb), *Name);
}

void UNWGatherComponent::InteractPressed()
{
	if (IsGathering())
	{
		ServerCancel();
	}
	else if (FocusCreature.IsValid() && !bFocusBlocked)
	{
		ServerSkin(FocusCreature.Get());
	}
	else if (FocusIndex != INDEX_NONE && !bFocusBlocked)
	{
		ServerGather(FocusIndex);
	}
}

void UNWGatherComponent::ServerGather_Implementation(int32 Index)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	if (!Me || !Field || IsGathering() || Index < 0 || Index >= Field->Num() || Field->IsDepleted(Index)
		|| Me->Combat->State != ENWCombatState::Idle || !Field->InReach(Index, Me->GetActorLocation(), 150.f))
	{
		return;
	}
	const ANWGatherField::FVariant& V = Field->VariantOf(Index);
	const FName Tool = ToolType(V.Action);
	const FNWItemDef* ToolDef = Tool.IsNone() || !Me->Inventory ? nullptr : Me->Inventory->ToolFor(Tool);
	if ((V.Restriction == TEXT("ToolRequired") && !Tool.IsNone() && !ToolDef)
		|| (Me->Progression && Me->Progression->MissingTradeLevel(V.Tradeskill, V.RequiredLevel) > 0))
	{
		return;
	}
	for (TObjectIterator<UNWGatherComponent> It; It; ++It)
	{
		if (*It != this && It->GetWorld() == GetWorld() && It->GatherIndex == Index)
		{
			if (ANWPlayerController* PC = Cast<ANWPlayerController>(Me->GetController()))
			{
				PC->Warning(NWText::Key(TEXT("nw.gather.busy")));
			}
			return;
		}
	}
	// Tempo do jogo: BaseGatherTime ÷ (eficiência da ferramenta + bônus do nível do ofício); à mão = tempo base.
	const float SkillBonus = Me->Progression ? Me->Progression->TradeEfficiency(NWTradeskill::Index(V.Tradeskill)) : 0.f;
	const float Time = FMath::Max(1.f, V.Time / FMath::Max(0.1f, (ToolDef ? ToolDef->Efficiency : 1.f) + SkillBonus));
	GatherIndex = Index;
	GatherAction = V.Action;
	GatherStart = ServerNow();
	GatherEnd = GatherStart + Time;
	GatherFrom = Me->GetActorLocation();
	// Vira para o coletável.
	const FVector To = Field->GetActorTransform().TransformPosition(Field->Node(Index).Transform.GetLocation()) - GatherFrom;
	Me->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	OnRep_Gather();
	GetOwner()->ForceNetUpdate();
}

void UNWGatherComponent::ServerSkin_Implementation(ANWCreature* Creature)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	const FNWCreatureDef* D = Creature ? Creature->Def() : nullptr;
	if (!Me || !D || IsGathering() || D->SkinLoot.IsNone() || Creature->bSkinned || Creature->SkinnedBy.IsValid()
		|| Creature->Combat->State != ENWCombatState::Dead || Me->Combat->State != ENWCombatState::Idle
		|| FVector::Dist2D(Creature->GetActorLocation(), Me->GetActorLocation()) > 300.f)
	{
		return;
	}
	const FNWItemDef* Knife = Me->Inventory ? Me->Inventory->ToolFor(TEXT("Dressing")) : nullptr;
	if (!Knife)
	{
		return;
	}
	const float SkillBonus = Me->Progression ? Me->Progression->TradeEfficiency(NWTradeskill::Index(D->SkinTradeskill)) : 0.f;
	Creature->SkinnedBy = Me;
	GatherIndex = SkinIndex;
	GatherActor = Creature;
	GatherAction = TEXT("Dressing");
	GatherStart = ServerNow();
	GatherEnd = GatherStart + FMath::Max(1.f, D->SkinTime / FMath::Max(0.1f, Knife->Efficiency + SkillBonus));
	GatherFrom = Me->GetActorLocation();
	const FVector To = Creature->GetActorLocation() - GatherFrom;
	Me->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	OnRep_Gather();
	GetOwner()->ForceNetUpdate();
}

void UNWGatherComponent::ServerCancel_Implementation()
{
	if (IsGathering())
	{
		Finish(false);
	}
}

void UNWGatherComponent::Finish(bool bComplete)
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	ANWGatherField* Field = ANWGatherField::Get(GetWorld());
	const int32 Index = GatherIndex;
	ANWCreature* Corpse = Cast<ANWCreature>(GatherActor.Get());
	GatherIndex = INDEX_NONE;
	GatherAction = NAME_None;
	GatherActor = nullptr;
	OnRep_Gather();
	GetOwner()->ForceNetUpdate();
	if (Index == SkinIndex)
	{
		// Carcaça: saque da linha de esfolar do jogo (BoarSkinning...) e XP de Esfolamento.
		const FNWCreatureDef* D = Corpse ? Corpse->Def() : nullptr;
		if (Corpse)
		{
			Corpse->SkinnedBy.Reset();
		}
		if (!bComplete || !Me || !D || Corpse->bSkinned)
		{
			return;
		}
		Corpse->bSkinned = true;
		Corpse->ForceNetUpdate();
		TArray<FNWItem> Gained;
		const int32 Bonus = Me->Progression ? Me->Progression->TradeRollBonus(NWTradeskill::Index(D->SkinTradeskill)) : 0;
		FNWItemCatalog::Get().RollGame(D->SkinLoot, Gained, Bonus);
		if (Me->Inventory)
		{
			const NWOps::FReason Reason(TEXT("gather"));   // log de itens (NWOps)
			Me->Inventory->Grant(Gained, 0);
		}
		if (Me->Progression)
		{
			Me->Progression->AddTradeXp(D->SkinTradeskill, D->SkinTradeXp);
		}
		return;
	}
	if (!bComplete || !Me || !Field || Field->IsDepleted(Index))
	{
		return;
	}
	TArray<FNWItem> Gained;
	// Nível do ofício soma no dado do saque (RollBonus) nas tabelas que aceitam (RollBonusSetting).
	const int32 Bonus = Me->Progression ? Me->Progression->TradeRollBonus(NWTradeskill::Index(Field->VariantOf(Index).Tradeskill)) : 0;
	FNWItemCatalog::Get().RollGame(Field->VariantOf(Index).Loot, Gained, Bonus);
	const ANWGatherField::FVariant& V = Field->VariantOf(Index);
	Field->Deplete(Index);
	if (Me->Inventory)
	{
		const NWOps::FReason Reason(TEXT("gather"));   // log de itens (NWOps)
		Me->Inventory->Grant(Gained, 0);
	}
	if (Me->Progression)
	{
		Me->Progression->AddTradeXp(V.Tradeskill, V.TradeXp);
	}
}

void UNWGatherComponent::OnRep_Gather()
{
	ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me)
	{
		return;
	}
	UAnimInstance* Anim = Me->GetMesh() ? Me->GetMesh()->GetAnimInstance() : nullptr;
	if (IsGathering())
	{
		// Ferramenta na mão direita, escudo guardado; a animação do jogo em loop.
		if (const TSoftObjectPtr<UStaticMesh>* Tool = ToolMeshes.Find(GatherAction))
		{
			if (UStaticMesh* M = Tool->LoadSynchronous())
			{
				Me->Sword->SetStaticMesh(M);
			}
		}
		else
		{
			Me->Sword->SetVisibility(false);
		}
		Me->Shield->SetVisibility(false);
		const TSoftObjectPtr<UAnimMontage>* Montage = Montages.Find(GatherAction);
		PlayingMontage = Montage ? Montage->LoadSynchronous() : nullptr;
		if (Anim && PlayingMontage)
		{
			Anim->Montage_Play(PlayingMontage, 1.f);
			const FName Section = PlayingMontage->GetSectionName(0);
			Anim->Montage_SetNextSection(Section, Section, PlayingMontage);
		}
	}
	else
	{
		if (Anim && PlayingMontage)
		{
			Anim->Montage_Stop(0.2f, PlayingMontage);
		}
		PlayingMontage = nullptr;
		Me->Sword->SetVisibility(true);
		Me->Shield->SetVisibility(true);
		Me->ApplyWeapon();
	}
}

void UNWGatherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ANWCharacter* Me = Cast<ANWCharacter>(GetOwner());
	if (!Me)
	{
		return;
	}
	// Foco do [E] só no jogador local: no servidor, criaturas e IA contam como "controladas localmente" e procuravam
	// coletável a cada ciclo (docs/tech/architecture.md, medições).
	if (Me->IsLocallyControlled() && Me->IsPlayerControlled())
	{
		UpdateFocus();
	}
	if (GetOwnerRole() != ROLE_Authority || !IsGathering())
	{
		return;
	}
	// Andar, sair do lugar, atacar/esquivar ou morrer cancela.
	const bool bMoved = Me->GetVelocity().Size2D() > 30.f || FVector::Dist2D(Me->GetActorLocation(), GatherFrom) > 60.f;
	if (bMoved || Me->Combat->State != ENWCombatState::Idle)
	{
		Finish(false);
	}
	else if (ServerNow() >= GatherEnd)
	{
		Finish(true);
	}
}
