#include "NWGameMode.h"
#include "NWLootBag.h"
#include "NWOps.h"

#include "NWText.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NWCharacter.h"
#include "NWGatherField.h"
#include "NWSpawnField.h"
#include "NWHUD.h"
#include "NWPlayerController.h"
#include "NWPlayerState.h"
#include "NWItems.h"
#include "NWCombatComponent.h"
#include "NWInventoryComponent.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWGameMode, Log, All);

ANWGameMode::ANWGameMode()
{
	DefaultPawnClass = ANWCharacter::StaticClass();
	HUDClass = ANWHUD::StaticClass();
	PlayerControllerClass = ANWPlayerController::StaticClass();
	PlayerStateClass = ANWPlayerState::StaticClass();
}

UNWBackend* ANWGameMode::Backend() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNWBackend>() : nullptr;
}

void ANWGameMode::BeginPlay()
{
	Super::BeginPlay();
	// Region gatherables (F4): the field replicates to the clients, which read the same file.
	if (!ANWGatherField::DataFileFor(GetWorld()).IsEmpty() && !ANWGatherField::Get(GetWorld()))
	{
		GetWorld()->SpawnActor<ANWGatherField>();
	}
	// Region creatures (G3): server only; the creatures replicate.
	if (!ANWSpawnField::DataFileFor(GetWorld()).IsEmpty())
	{
		GetWorld()->SpawnActor<ANWSpawnField>();
	}
	// Editor Play alone: local test nw-core with the editor account (docs/tech/backend-testing.md). The local player has
	// already joined (PostLogin comes before BeginPlay in Play).
	UNWBackend* PieBackend = Backend();
	if (GetWorld()->WorldType == EWorldType::PIE && GetNetMode() == NM_Standalone && PieBackend && PieBackend->EnablePie())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			PieEnter(Cast<ANWPlayerController>(It->Get()));
		}
	}
	if (UNWBackend* B = Backend(); B && B->IsServerEnabled())
	{
		GetWorldTimerManager().SetTimer(SaveTimer, this, &ANWGameMode::SavePeriodic, SaveInterval, true);
		// Save before the actors leave (at the end of Play the pawn may vanish before the GameMode's EndPlay).
		TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ANWGameMode::HandleTearDown);
	}
}

void ANWGameMode::HandleTearDown(UWorld* World)
{
	if (World == GetWorld())
	{
		SaveAll(TEXT("shutdown"));
	}
}

void ANWGameMode::PieEnter(ANWPlayerController* PC)
{
	UNWBackend* B = Backend();
	if (!PC || PC->Record || !PC->IsLocalController() || !B || !B->IsPie())
	{
		return;
	}
	B->PieEnter([WeakThis = TWeakObjectPtr<ANWGameMode>(this), WeakPC = TWeakObjectPtr<ANWPlayerController>(PC)](TOptional<FNWCharacterRecord> Record, const FString& Error)
	{
		ANWGameMode* Self = WeakThis.Get();
		ANWPlayerController* P = WeakPC.Get();
		if (!Self || !P)
		{
			return;
		}
		if (!Record)
		{
			// nw-core down: continue without backend (PostLogin starting kit), with a warning.
			UE_LOG(LogNWGameMode, Warning, TEXT("Playing without backend: %s"), *Error);
			if (UNWBackend* B = Self->Backend())
			{
				B->DisablePie();
			}
			Self->GetWorldTimerManager().ClearTimer(Self->SaveTimer);
			P->Warning(NWText::Key(TEXT("nw.pie.offline"), {Error}));
			return;
		}
		UE_LOG(LogNWGameMode, Display, TEXT("Playing with backend: %s (%s)"), *Record->Name, *Record->AccountName);
		if (APlayerState* PS = P->GetPlayerState<APlayerState>())
		{
			PS->SetPlayerName(Record->Name);
		}
		P->Record = MoveTemp(*Record);
		P->bPlacedFromRecord = true;
		P->UpdateDevAllowed();
		ANWLootBag::RestoreFor(P);   // bags on the ground still alive come back
		if (ANWCharacter* Ch = Cast<ANWCharacter>(P->GetPawn()))
		{
			const FNWCharacterRecord& R = *P->Record;
			if (R.bHasPosition && R.Map == UWorld::RemovePIEPrefix(Self->GetWorld()->GetMapName()))
			{
				Ch->TeleportTo(R.Position, FRotator(0.f, R.Yaw, 0.f));
				P->SetControlRotation(FRotator(0.f, R.Yaw, 0.f));
			}
			Ch->RestoreFrom(R);
			Ch->CaptureTo(*P->Record);
		}
		P->PublishProgress();
	});
}

void ANWGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	SaveAll(TEXT("shutdown"));
	Super::EndPlay(Reason);
}

bool ANWGameMode::IsCharacterOnline(const FString& CharacterId) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ANWPlayerController* PC = Cast<ANWPlayerController>(It->Get());
		if (PC && PC->Record && PC->Record->Id == CharacterId)
		{
			return true;
		}
	}
	return false;
}

void ANWGameMode::PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete)
{
	UNWBackend* B = Backend();
	if (!B || !B->IsServerEnabled())
	{
		Super::PreLoginAsync(Options, Address, UniqueId, OnComplete);
		return;
	}
	FString Error;
	PreLogin(Options, Address, UniqueId, Error);
	const FString Ticket = UGameplayStatics::ParseOption(Options, TEXT("ticket"));
	if (Error.IsEmpty() && Ticket.IsEmpty())
	{
		Error = NWText::Key(TEXT("nw.err.no_ticket"));
	}
	if (!Error.IsEmpty())
	{
		OnComplete.ExecuteIfBound(Error);
		return;
	}
	B->RedeemTicket(Ticket, [WeakThis = TWeakObjectPtr<ANWGameMode>(this), Ticket, Address, OnComplete](TOptional<FNWCharacterRecord> Record, const FString& Err)
	{
		ANWGameMode* Self = WeakThis.Get();
		if (!Self)
		{
			OnComplete.ExecuteIfBound(NWText::Key(TEXT("nw.err.server_closing")));
			return;
		}
		if (!Record)
		{
			UE_LOG(LogNWGameMode, Warning, TEXT("join refused (%s): %s"), *Address, *Err);
			OnComplete.ExecuteIfBound(Err);
			return;
		}
		if (Self->IsCharacterOnline(Record->Id))
		{
			OnComplete.ExecuteIfBound(NWText::Key(TEXT("nw.err.already_online")));
			return;
		}
		UE_LOG(LogNWGameMode, Display, TEXT("join: %s (%s) from %s"), *Record->Name, *Record->AccountName, *Address);
		Self->Pending.Add(Ticket, MoveTemp(*Record));
		OnComplete.ExecuteIfBound(FString());
	});
}

FString ANWGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	UNWBackend* B = Backend();
	if (!Error.IsEmpty() || !B || !B->IsServerEnabled() || NewPlayerController->IsLocalController())
	{
		return Error;
	}
	FNWCharacterRecord Record;
	if (!Pending.RemoveAndCopyValue(UGameplayStatics::ParseOption(Options, TEXT("ticket")), Record))
	{
		return NWText::Key(TEXT("nw.err.ticket_missing"));
	}
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(NewPlayerController))
	{
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			PS->SetPlayerName(Record.Name);
		}
		PC->Record = MoveTemp(Record);
		PC->UpdateDevAllowed();
		ANWLootBag::RestoreFor(PC);   // bags on the ground still alive come back
	}
	return Error;
}

void ANWGameMode::RestartPlayer(AController* NewPlayer)
{
	ANWPlayerController* PC = Cast<ANWPlayerController>(NewPlayer);
	// Only the first time: after dying, spawns at the map start.
	if (PC && PC->Record && !PC->bPlacedFromRecord)
	{
		PC->bPlacedFromRecord = true;
		const FNWCharacterRecord& R = *PC->Record;
		if (R.bHasPosition && R.Map == UWorld::RemovePIEPrefix(GetWorld()->GetMapName()))
		{
			RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.f, R.Yaw, 0.f), R.Position));
			if (NewPlayer->GetPawn())
			{
				return;
			}
			UE_LOG(LogNWGameMode, Warning, TEXT("%s did not fit at the saved position; going to the start"), *R.Name);
		}
	}
	Super::RestartPlayer(NewPlayer);
}

void ANWGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ANWPlayerController* PC = Cast<ANWPlayerController>(NewPlayer);
	PieEnter(PC);   // Editor Play with backend (only after BeginPlay; before that, BeginPlay calls it)
	if (PC && !PC->Record)
	{
		// No backend (editor Play, local test): starting kit with the gathering tools, so there is something to test.
		if (ANWCharacter* Ch = Cast<ANWCharacter>(PC->GetPawn()); Ch && Ch->Inventory && Ch->Inventory->Items.Num() == 0)
		{
			Ch->Inventory->GiveStarterKit();
		}
		return;
	}
	if (!PC)
	{
		return;
	}
	if (ANWCharacter* Ch = Cast<ANWCharacter>(PC->GetPawn()))
	{
		Ch->RestoreFrom(*PC->Record);
		Ch->CaptureTo(*PC->Record);  // effective abilities (default when the record comes empty)
	}
	PC->PublishProgress();
}

void ANWGameMode::Logout(AController* Exiting)
{
	if (ANWPlayerController* PC = Cast<ANWPlayerController>(Exiting))
	{
		GroupLeave(PC);
		SavePlayer(PC, TEXT("logout"));
		ANWLootBag::RemoveFor(PC);   // stay in nw-core until they expire
		if (PC->Record)
		{
			UE_LOG(LogNWGameMode, Display, TEXT("leave: %s"), *PC->Record->Name);
		}
	}
	Super::Logout(Exiting);
}

void ANWGameMode::SavePlayer(ANWPlayerController* PC, const FString& Reason)
{
	UNWBackend* B = Backend();
	if (!PC || !PC->Record || !B || !B->IsServerEnabled())
	{
		return;
	}
	// Operation being written (NWOps): saving now would write the state from before it; save afterwards.
	if (ANWCharacter* Busy = Cast<ANWCharacter>(PC->GetPawn()); Busy && NWOps::IsBusy(Busy))
	{
		TWeakObjectPtr<ANWGameMode> Self(this);
		TWeakObjectPtr<ANWPlayerController> WeakPC(PC);
		NWOps::After(Busy, [Self, WeakPC, Reason]() { if (Self.IsValid() && WeakPC.IsValid()) { Self->SavePlayer(WeakPC.Get(), Reason); } });
		return;
	}
	FNWCharacterRecord& R = *PC->Record;
	R.Map = UWorld::RemovePIEPrefix(GetWorld()->GetMapName());
	if (const ANWCharacter* Ch = Cast<ANWCharacter>(PC->GetPawn()))
	{
		Ch->CaptureTo(R);
	}
	B->SaveState(R, Reason);
}

void ANWGameMode::SaveAll(const TCHAR* Reason)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		SavePlayer(Cast<ANWPlayerController>(It->Get()), Reason);
	}
}

// ---------------------------------------------------------------- groups

TArray<ANWPlayerState*> ANWGameMode::GroupMembers(int32 GroupId) const
{
	TArray<ANWPlayerState*> Out;
	if (GroupId != 0 && GameState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (ANWPlayerState* N = Cast<ANWPlayerState>(PS); N && N->GroupId == GroupId)
			{
				Out.Add(N);
			}
		}
	}
	return Out;
}

ANWPlayerState* ANWGameMode::FindPlayer(const FString& Name) const
{
	if (GameState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (PS && PS->GetPlayerName().Equals(Name.TrimStartAndEnd(), ESearchCase::IgnoreCase))
			{
				return Cast<ANWPlayerState>(PS);
			}
		}
	}
	return nullptr;
}

static ANWPlayerController* ControllerOf(const APlayerState* PS)
{
	return PS ? Cast<ANWPlayerController>(PS->GetOwner()) : nullptr;
}

void ANWGameMode::GroupSystem(int32 GroupId, const FString& Message)
{
	for (ANWPlayerState* M : GroupMembers(GroupId))
	{
		if (ANWPlayerController* PC = ControllerOf(M))
		{
			PC->SystemMessage(Message);
		}
	}
}

void ANWGameMode::GroupInvite(ANWPlayerController* From, const FString& TargetName)
{
	ANWPlayerState* Me = From->GetPlayerState<ANWPlayerState>();
	ANWPlayerState* Target = FindPlayer(TargetName);
	if (!Me)
	{
		return;
	}
	if (!Target || Target == Me)
	{
		From->SystemMessage(NWText::Key(TEXT("nw.group.not_found"), {TargetName}));
		return;
	}
	if (Me->GroupId != 0 && !Me->bGroupLeader)
	{
		From->SystemMessage(NWText::Key(TEXT("nw.group.only_leader")));
		return;
	}
	if (Target->GroupId != 0)
	{
		From->SystemMessage(NWText::Key(TEXT("nw.group.already_grouped"), {Target->GetPlayerName()}));
		return;
	}
	if (GroupMembers(Me->GroupId).Num() >= MaxGroupSize)
	{
		From->SystemMessage(NWText::Key(TEXT("nw.group.full")));
		return;
	}
	Invites.Add(Target, {Me, FPlatformTime::Seconds() + 60.0});
	From->SystemMessage(NWText::Key(TEXT("nw.group.invite_sent"), {Target->GetPlayerName()}));
	if (ANWPlayerController* TPC = ControllerOf(Target))
	{
		TPC->SystemMessage(NWText::Key(TEXT("nw.group.invited"), {Me->GetPlayerName()}));
	}
}

void ANWGameMode::GroupAccept(ANWPlayerController* PC)
{
	ANWPlayerState* Me = PC->GetPlayerState<ANWPlayerState>();
	const FInvite* Invite = Me ? Invites.Find(Me) : nullptr;
	ANWPlayerState* Leader = Invite ? Invite->From.Get() : nullptr;
	if (!Invite || !Leader || FPlatformTime::Seconds() > Invite->Expires)
	{
		Invites.Remove(Me);
		PC->SystemMessage(NWText::Key(TEXT("nw.group.no_invite")));
		return;
	}
	Invites.Remove(Me);
	if (Me->GroupId != 0)
	{
		GroupLeave(PC);
	}
	if (Leader->GroupId == 0)
	{
		Leader->GroupId = NextGroupId++;
		Leader->bGroupLeader = true;
		Leader->ForceNetUpdate();
	}
	if (GroupMembers(Leader->GroupId).Num() >= MaxGroupSize)
	{
		PC->SystemMessage(NWText::Key(TEXT("nw.group.full")));
		return;
	}
	Me->GroupId = Leader->GroupId;
	Me->bGroupLeader = false;
	Me->ForceNetUpdate();
	UE_LOG(LogNWGameMode, Display, TEXT("group %d: %s joined (leader %s)"), Me->GroupId, *Me->GetPlayerName(), *Leader->GetPlayerName());
	GroupSystem(Me->GroupId, NWText::Key(TEXT("nw.group.joined"), {Me->GetPlayerName()}));
}

void ANWGameMode::GroupDecline(ANWPlayerController* PC)
{
	ANWPlayerState* Me = PC->GetPlayerState<ANWPlayerState>();
	if (const FInvite* Invite = Me ? Invites.Find(Me) : nullptr)
	{
		if (ANWPlayerController* From = ControllerOf(Invite->From.Get()))
		{
			From->SystemMessage(NWText::Key(TEXT("nw.group.declined"), {Me->GetPlayerName()}));
		}
		Invites.Remove(Me);
	}
}

void ANWGameMode::GroupLeave(ANWPlayerController* PC)
{
	ANWPlayerState* Me = PC ? PC->GetPlayerState<ANWPlayerState>() : nullptr;
	if (!Me || Me->GroupId == 0)
	{
		return;
	}
	const int32 Id = Me->GroupId;
	const bool bWasLeader = Me->bGroupLeader;
	Me->GroupId = 0;
	Me->bGroupLeader = false;
	Me->ForceNetUpdate();
	PC->SystemMessage(NWText::Key(TEXT("nw.group.you_left")));
	TArray<ANWPlayerState*> Rest = GroupMembers(Id);
	if (Rest.Num() <= 1)
	{
		// One left: the group ends.
		for (ANWPlayerState* M : Rest)
		{
			M->GroupId = 0;
			M->bGroupLeader = false;
			M->ForceNetUpdate();
			if (ANWPlayerController* MPC = ControllerOf(M))
			{
				MPC->SystemMessage(NWText::Key(TEXT("nw.group.disbanded")));
			}
		}
		return;
	}
	GroupSystem(Id, NWText::Key(TEXT("nw.group.left"), {Me->GetPlayerName()}));
	if (bWasLeader)
	{
		Rest[0]->bGroupLeader = true;
		Rest[0]->ForceNetUpdate();
		GroupSystem(Id, NWText::Key(TEXT("nw.group.new_leader"), {Rest[0]->GetPlayerName()}));
	}
}

void ANWGameMode::GroupChat(ANWPlayerController* From, const FString& Text)
{
	ANWPlayerState* Me = From->GetPlayerState<ANWPlayerState>();
	if (!Me || Me->GroupId == 0)
	{
		From->SystemMessage(NWText::Key(TEXT("nw.group.not_in_group")));
		return;
	}
	for (ANWPlayerState* M : GroupMembers(Me->GroupId))
	{
		if (ANWPlayerController* PC = ControllerOf(M))
		{
			PC->ClientChat(ENWChatChannel::Group, Me->GetPlayerName(), Text);
		}
	}
}

// ---------------------------------------------------------------- loot

void ANWGameMode::GrantLoot(APawn* Killer, const AActor* Victim, FName Table)
{
	ANWPlayerState* KillerPS = Killer ? Killer->GetPlayerState<ANWPlayerState>() : nullptr;
	if (!KillerPS)
	{
		return;
	}
	TArray<ANWPlayerState*> Recipients = KillerPS->GroupId != 0 ? GroupMembers(KillerPS->GroupId) : TArray<ANWPlayerState*>{KillerPS};
	for (ANWPlayerState* PS : Recipients)
	{
		ANWCharacter* Pawn = Cast<ANWCharacter>(PS->GetPawn());
		if (!Pawn || !Pawn->Inventory || Pawn->Combat->State == ENWCombatState::Dead
			|| (Victim && FVector::Dist(Pawn->GetActorLocation(), Victim->GetActorLocation()) > 6000.f))
		{
			continue;
		}
		TArray<FNWItem> Gained;
		int64 Coins = 0;
		FNWItemCatalog::Get().RollLoot(Table, Gained, Coins);
		const NWOps::FReason Reason(TEXT("loot:") + Table.ToString());
		Pawn->Inventory->Grant(Gained, Coins);
	}
}
