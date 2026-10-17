#include "NWAdmin.h"

#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWBehaviorTreeComponent.h"
#include "NWBrainComponent.h"
#include "NWCharacter.h"
#include "NWCity.h"
#include "NWCombatComponent.h"
#include "NWCreature.h"
#include "NWDevRegistry.h"
#include "NWGameMode.h"
#include "NWInventoryComponent.h"
#include "NWOps.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if !UE_BUILD_SHIPPING
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogNWAdmin, Log, All);

namespace
{
	const FName BotTag(TEXT("NWBot"));
	const FName DevTag(TEXT("NWDev"));   // same tag as the F10 summons: "Remove spawned" also removes these

	float Now(const UWorld* W) { return W ? W->GetTimeSeconds() : 0.f; }

	bool Alive(const AActor* A)
	{
		const ANWCharacter* C = Cast<ANWCharacter>(A);
		return C && C->Combat && C->Combat->State != ENWCombatState::Dead;
	}

	/** Turns the character toward a point (bots: the server owns the rotation; players: their client does). */
	void FacePoint(ANWCharacter* Me, const FVector& At, float DeltaSeconds)
	{
		const FVector To = (At - Me->GetActorLocation()).GetSafeNormal2D();
		if (To.IsNearlyZero())
		{
			return;
		}
		const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
		Me->SetActorRotation(FMath::RInterpConstantTo(Me->GetActorRotation(), Want, DeltaSeconds, 720.f));
		if (AController* C = Me->GetController(); C && C->IsLocalPlayerController())
		{
			C->SetControlRotation(FRotator(C->GetControlRotation().Pitch, Want.Yaw, 0.f));
		}
	}
}

// ---------------------------------------------------------------- drive

FString NWAdmin::ModeName(FNWDrive::EMode Mode)
{
	switch (Mode)
	{
	case FNWDrive::EMode::MoveTo: return TEXT("move");
	case FNWDrive::EMode::Follow: return TEXT("follow");
	case FNWDrive::EMode::Fight: return TEXT("fight");
	case FNWDrive::EMode::Wander: return TEXT("wander");
	default: return TEXT("idle");
	}
}

bool NWAdmin::ParseMode(const FString& Name, FNWDrive::EMode& Out)
{
	for (FNWDrive::EMode M : {FNWDrive::EMode::None, FNWDrive::EMode::MoveTo, FNWDrive::EMode::Follow, FNWDrive::EMode::Fight, FNWDrive::EMode::Wander})
	{
		if (ModeName(M) == Name || (Name == TEXT("stop") && M == FNWDrive::EMode::None))
		{
			Out = M;
			return true;
		}
	}
	return false;
}

void NWAdmin::TickDrive(ANWCharacter* Me, FNWDrive& D, float DeltaSeconds)
{
	if (!Me || !Me->Combat || Me->Combat->State == ENWCombatState::Dead)
	{
		return;
	}
	const UWorld* W = Me->GetWorld();
	const double T = Now(W);
	const FVector Pos = Me->GetActorLocation();
	auto MoveToward = [&](const FVector& Dest, float Stop) -> bool
	{
		FVector To = Dest - Pos;
		To.Z = 0.f;
		if (To.SizeSquared() <= Stop * Stop)
		{
			return true;
		}
		Me->AddMovementInput(To.GetSafeNormal(), 1.f);
		return false;
	};
	switch (D.Mode)
	{
	case FNWDrive::EMode::MoveTo:
		if (MoveToward(D.Goal, 150.f))
		{
			D.Mode = FNWDrive::EMode::None;
		}
		break;
	case FNWDrive::EMode::Follow:
		if (const AActor* Who = D.Target.Get())
		{
			MoveToward(Who->GetActorLocation(), 350.f);
		}
		else
		{
			D.Mode = FNWDrive::EMode::None;
		}
		break;
	case FNWDrive::EMode::Wander:
		if (T >= D.NextGoal || MoveToward(D.Goal, 150.f))
		{
			const FVector2D Off = FMath::RandPointInCircle(2500.f);
			D.Goal = D.Home + FVector(Off.X, Off.Y, 0.f);
			D.NextGoal = T + FMath::FRandRange(4.0, 8.0);
		}
		MoveToward(D.Goal, 150.f);
		break;
	case FNWDrive::EMode::Fight:
	{
		AActor* Who = D.Target.Get();
		if (!Alive(Who))
		{
			D.Mode = FNWDrive::EMode::None;
			break;
		}
		const FVector At = Who->GetActorLocation();
		const float Reach = 230.f;
		if (!MoveToward(At, Reach) || Me->Combat->IsBusy())
		{
			break;
		}
		FacePoint(Me, At, DeltaSeconds);
		if (T >= D.NextAttack)
		{
			// Mostly light combos, sometimes a heavy or an ability (like the load-test bots).
			const float Roll = FMath::FRand();
			if (Roll < 0.15f && Me->Abilities.Num() > 0)
			{
				Me->Combat->RequestAction(Me->Abilities[FMath::RandHelper(Me->Abilities.Num())]);
			}
			else if (Roll < 0.3f)
			{
				Me->Combat->RequestHeavyAttack();
			}
			else
			{
				Me->Combat->RequestLightAttack();
			}
			D.NextAttack = T + FMath::FRandRange(0.6, 1.3);
		}
		break;
	}
	default:
		break;
	}
}

bool NWAdmin::Act(ANWCharacter* Me, FName Action)
{
	if (!Me || !Me->Combat)
	{
		return false;
	}
	const FString A = Action.ToString();
	if (A == TEXT("light")) { Me->Combat->RequestLightAttack(); return true; }
	if (A == TEXT("heavy")) { Me->Combat->RequestHeavyAttack(); return true; }
	if (A == TEXT("dodge")) { Me->Combat->RequestDodge(FVector2D(-1.f, 0.f)); return true; }
	if (A == TEXT("block")) { Me->Combat->RequestBlock(true); return true; }
	if (A == TEXT("unblock")) { Me->Combat->RequestBlock(false); return true; }
	if (A == TEXT("swap")) { Me->SwitchWeapon(Me->OtherWeapon()); return true; }
	if (A == TEXT("jump"))
	{
		Me->Jump();
		FTimerHandle H;
		Me->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(Me, [Me]() { Me->StopJumping(); }), 0.3f, false);
		return true;
	}
	if (A.StartsWith(TEXT("ability")))
	{
		const int32 I = FCString::Atoi(*A.RightChop(7)) - 1;
		if (Me->Abilities.IsValidIndex(I))
		{
			Me->Combat->RequestAction(Me->Abilities[I]);
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- bots

UNWAdminSubsystem* UNWAdminSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UNWAdminSubsystem>() : nullptr;
}

bool UNWAdminSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer);
#endif
}

bool UNWAdminSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

ANWCharacter* UNWAdminSubsystem::SpawnBot(const FVector& Near, const FString& Name)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	// Ground under a point 3–6 m around the reference.
	FVector Where = Near;
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FVector2D Off = FMath::RandPointInCircle(300.f) + FVector2D(300.f, 0.f).GetRotated(FMath::FRandRange(0.f, 360.f));
		const FVector Top = Near + FVector(Off.X, Off.Y, 2000.f);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 6000.f), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(NWAdminGround), false)))
		{
			Where = Hit.ImpactPoint + FVector(0.f, 0.f, 100.f);
			break;
		}
	}
	const FTransform At(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Where);
	ANWCharacter* C = World->SpawnActorDeferred<ANWCharacter>(ANWCharacter::StaticClass(), At, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!C)
	{
		return nullptr;
	}
	// Like the load-test players: creatures and AI treat it as a player.
	C->bSimPlayer = true;
	C->Tags.Add(BotTag);
	C->FinishSpawning(At);
	FBot B;
	B.Pawn = C;
	B.Name = Name.IsEmpty() ? FString::Printf(TEXT("Bot %d"), NextBotNumber) : Name;
	B.Drive.Home = Where;
	++NextBotNumber;
	BotList.Add(MoveTemp(B));
	UE_LOG(LogNWAdmin, Display, TEXT("Bot created: %s at %s"), *BotList.Last().Name, *Where.ToCompactString());
	return C;
}

UNWAdminSubsystem::FBot* UNWAdminSubsystem::FindBot(const AActor* Pawn)
{
	return BotList.FindByPredicate([Pawn](const FBot& B) { return B.Pawn.Get() == Pawn; });
}

void UNWAdminSubsystem::RemoveBot(const AActor* Pawn)
{
	BotList.RemoveAll([Pawn](const FBot& B)
	{
		if (B.Pawn.Get() == Pawn)
		{
			B.Pawn->Destroy();
			return true;
		}
		return false;
	});
}

void UNWAdminSubsystem::Tick(float DeltaTime)
{
	BotList.RemoveAll([](const FBot& B) { return !B.Pawn.IsValid(); });
	for (FBot& B : BotList)
	{
		NWAdmin::TickDrive(B.Pawn.Get(), B.Drive, DeltaTime);
	}
}

// ---------------------------------------------------------------- HTTP API

#if !UE_BUILD_SHIPPING
namespace
{
	UWorld* ServerWorld()
	{
		for (const FWorldContext& C : GEngine ? GEngine->GetWorldContexts() : TIndirectArray<FWorldContext>())
		{
			UWorld* W = C.World();
			if (W && (C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE) && W->GetNetMode() != NM_Client)
			{
				return W;
			}
		}
		return nullptr;
	}

	AActor* FindActor(UWorld* W, int64 Id)
	{
		if (Id <= 0)
		{
			return nullptr;
		}
		for (TActorIterator<ANWCharacter> It(W); It; ++It)
		{
			if (int64(It->GetUniqueID()) == Id)
			{
				return *It;
			}
		}
		return nullptr;
	}

	ANWPlayerController* PlayerOf(const AActor* A)
	{
		const APawn* P = Cast<APawn>(A);
		return P ? Cast<ANWPlayerController>(P->GetController()) : nullptr;
	}

	FString Kind(const ANWCharacter* C, UNWAdminSubsystem* Admin)
	{
		if (PlayerOf(C)) return TEXT("player");
		if (Admin && Admin->FindBot(C)) return TEXT("bot");
		if (C->bSimPlayer) return TEXT("sim");
		if (C->IsA<ANWCreature>()) return TEXT("creature");
		return TEXT("npc");
	}

	TArray<TSharedPtr<FJsonValue>> Vec(const FVector& V)
	{
		return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Y)),
			MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Z))};
	}

	FString ToJson(const TSharedRef<FJsonObject>& O)
	{
		FString Out;
		FJsonSerializer::Serialize(O, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
		return Out;
	}

	TSharedRef<FJsonObject> Describe(ANWCharacter* C, UNWAdminSubsystem* Admin)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		const FString K = Kind(C, Admin);
		O->SetNumberField(TEXT("id"), C->GetUniqueID());
		O->SetStringField(TEXT("kind"), K);
		O->SetArrayField(TEXT("pos"), Vec(C->GetActorLocation()));
		O->SetNumberField(TEXT("yaw"), FMath::RoundToDouble(C->GetActorRotation().Yaw));
		O->SetNumberField(TEXT("hp"), FMath::RoundToDouble(C->Combat->Health));
		O->SetNumberField(TEXT("max_hp"), FMath::RoundToDouble(C->Combat->MaxHealth));
		O->SetStringField(TEXT("state"), StaticEnum<ENWCombatState>()->GetNameStringByValue(int64(C->Combat->State)));
		O->SetStringField(TEXT("action"), C->Combat->GetCurrentAction().ToString());
		O->SetBoolField(TEXT("god"), C->Combat->bGodMode);
		O->SetBoolField(TEXT("paused"), C->CustomTimeDilation < 0.01f);
		O->SetNumberField(TEXT("damage"), C->Combat->WeaponBaseDamage);
		O->SetNumberField(TEXT("speed"), C->DevSpeedMultiplier);
		O->SetNumberField(TEXT("aggro_range"), C->AggroRange);
		if (C->WeaponSets.IsValidIndex(C->ActiveWeapon))
		{
			O->SetStringField(TEXT("weapon"), C->WeaponSets[C->ActiveWeapon].Id.ToString());
		}
		if (ANWPlayerController* PC = PlayerOf(C))
		{
			O->SetStringField(TEXT("name"), PC->Record ? PC->Record->Name : PC->GetName());
			O->SetStringField(TEXT("account"), PC->Record ? PC->Record->AccountName : FString());
			O->SetStringField(TEXT("character_id"), PC->Record ? PC->Record->Id : FString());
			O->SetBoolField(TEXT("admin"), PC->bDevAllowed);
			O->SetStringField(TEXT("drive"), PC->AdminDriveShown.IsEmpty() ? TEXT("idle") : PC->AdminDriveShown);
			O->SetBoolField(TEXT("remote"), PC->GetNetConnection() != nullptr);
		}
		else if (UNWAdminSubsystem::FBot* B = Admin ? Admin->FindBot(C) : nullptr)
		{
			O->SetStringField(TEXT("name"), B->Name);
			O->SetStringField(TEXT("drive"), NWAdmin::ModeName(B->Drive.Mode));
			O->SetNumberField(TEXT("drive_target"), B->Drive.Target.IsValid() ? B->Drive.Target->GetUniqueID() : 0);
		}
		else if (const ANWCreature* Cr = Cast<ANWCreature>(C))
		{
			O->SetStringField(TEXT("name"), Cr->CreatureId.ToString());
			if (const FNWCreatureDef* D = Cr->Def())
			{
				O->SetNumberField(TEXT("level"), D->Level);
				O->SetStringField(TEXT("set"), D->Set.ToString());
			}
		}
		else
		{
			O->SetStringField(TEXT("name"), C->GetClass()->GetName().Replace(TEXT("NW"), TEXT("")));
		}
		if (C->Inventory && K != TEXT("creature"))
		{
			O->SetNumberField(TEXT("coins"), C->Inventory->Coins);
			O->SetNumberField(TEXT("max_weight"), C->Inventory->MaxWeight);
			O->SetBoolField(TEXT("dev_bag"), C->Inventory->bDevBag);
		}
		if (C->Progression && K != TEXT("creature"))
		{
			O->SetNumberField(TEXT("level"), C->Progression->Level);
		}
		if (const UNWBehaviorTreeComponent* BT = C->FindComponentByClass<UNWBehaviorTreeComponent>(); BT && BT->IsActive())
		{
			O->SetStringField(TEXT("ai"), TEXT("tree"));
			O->SetNumberField(TEXT("target"), BT->GetTarget() ? BT->GetTarget()->GetUniqueID() : 0);
			O->SetBoolField(TEXT("frozen"), BT->IsAdminFrozen());
		}
		else if (C->Brain && C->Brain->IsActive())
		{
			O->SetStringField(TEXT("ai"), TEXT("brain"));
			O->SetNumberField(TEXT("target"), C->Brain->GetTarget() ? C->Brain->GetTarget()->GetUniqueID() : 0);
			O->SetStringField(TEXT("style"), StaticEnum<ENWBrainStyle>()->GetNameStringByValue(int64(C->Brain->Style)));
			O->SetNumberField(TEXT("block_chance"), C->Brain->BlockChance);
			O->SetNumberField(TEXT("dodge_chance"), C->Brain->DodgeChance);
			O->SetNumberField(TEXT("reaction"), C->Brain->ReactionTime);
			O->SetNumberField(TEXT("hold_distance"), C->Brain->HoldDistance);
		}
		return O;
	}

	TSharedRef<FJsonObject> State(UWorld* W)
	{
		UNWAdminSubsystem* Admin = UNWAdminSubsystem::Get(W);
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("map"), W->GetMapName().Replace(*W->StreamingLevelsPrefix, TEXT("")));
		O->SetNumberField(TEXT("port"), W->URL.Port);
		O->SetNumberField(TEXT("time"), W->GetTimeSeconds());
		O->SetNumberField(TEXT("time_dilation"), UGameplayStatics::GetGlobalTimeDilation(W));
		O->SetNumberField(TEXT("coins_max"), NWCity::Rules().CoinsMax);
		TArray<TSharedPtr<FJsonValue>> Actors;
		for (TActorIterator<ANWCharacter> It(W); It; ++It)
		{
			if (It->Combat && !It->IsPendingKillPending())
			{
				Actors.Add(MakeShared<FJsonValueObject>(Describe(*It, Admin)));
			}
		}
		O->SetArrayField(TEXT("actors"), Actors);
		return O;
	}

	/** Lists that do not change while the server runs: F10 commands, named commands, creature catalog. */
	TSharedRef<FJsonObject> Catalog()
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Dev;
		const UEnum* E = StaticEnum<ENWDevCommand>();
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			Dev.Add(MakeShared<FJsonValueString>(E->GetNameStringByIndex(i)));
		}
		O->SetArrayField(TEXT("dev"), Dev);
		TArray<TSharedPtr<FJsonValue>> Named;
		for (const FNWDevEntry& D : FNWDevRegistry::All())
		{
			Named.Add(MakeShared<FJsonValueString>(D.Id.ToString()));
		}
		O->SetArrayField(TEXT("dev_named"), Named);
		ANWCreature::FindDef(NAME_None);   // loads the catalog
		TArray<TSharedPtr<FJsonValue>> Creatures;
		for (const TPair<FName, FNWCreatureDef>& P : FNWCreatureCatalog::Get().All())
		{
			if (P.Value.Set.IsNone())
			{
				continue;   // not converted: no mesh
			}
			TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
			C->SetStringField(TEXT("id"), P.Key.ToString());
			C->SetStringField(TEXT("set"), P.Value.Set.ToString());
			C->SetNumberField(TEXT("level"), P.Value.Level);
			C->SetBoolField(TEXT("tree"), FNWBTAsset::Get(P.Value.Set) != nullptr);
			Creatures.Add(MakeShared<FJsonValueObject>(C));
		}
		O->SetArrayField(TEXT("creatures"), Creatures);
		TArray<TSharedPtr<FJsonValue>> Styles;
		const UEnum* S = StaticEnum<ENWBrainStyle>();
		for (int32 i = 0; i < S->NumEnums() - 1; ++i)
		{
			Styles.Add(MakeShared<FJsonValueString>(S->GetNameStringByIndex(i)));
		}
		O->SetArrayField(TEXT("styles"), Styles);
		TArray<TSharedPtr<FJsonValue>> Weapons;
		for (const FNWWeaponSet& Set : GetDefault<ANWCharacter>()->WeaponSets)
		{
			Weapons.Add(MakeShared<FJsonValueString>(Set.Id.ToString()));
		}
		O->SetArrayField(TEXT("weapons"), Weapons);
		return O;
	}

	FVector PointNear(const AActor* A, float Ahead)
	{
		return A->GetActorLocation() + A->GetActorForwardVector() * Ahead + FVector(0.f, 0.f, 80.f);
	}

	void Save(ANWPlayerController* PC, const FString& Why)
	{
		if (ANWGameMode* GM = PC && PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<ANWGameMode>() : nullptr)
		{
			GM->SavePlayer(PC, TEXT("admin:") + Why);
		}
	}

	/** Runs one command. Returns an error text, empty when it worked. */
	FString Command(UWorld* W, const TSharedPtr<FJsonObject>& In, TSharedRef<FJsonObject>& Out)
	{
		UNWAdminSubsystem* Admin = UNWAdminSubsystem::Get(W);
		const FString Op = In->GetStringField(TEXT("op"));
		const int64 Id = int64(In->GetNumberField(TEXT("id")));
		ANWCharacter* Who = Cast<ANWCharacter>(FindActor(W, Id));
		ANWPlayerController* PC = PlayerOf(Who);
		ANWCharacter* Other = Cast<ANWCharacter>(FindActor(W, int64(In->GetNumberField(TEXT("target")))));
		const NWOps::FReason Reason(TEXT("admin:") + Op);
		if (Op != TEXT("ai"))   // the page polls the AI of the selected creature
		{
			UE_LOG(LogNWAdmin, Display, TEXT("admin %s id=%lld"), *Op, Id);
		}

		auto ReadVec = [&](const TCHAR* Key, FVector& V) -> bool
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!In->TryGetArrayField(Key, A) || A->Num() < 2)
			{
				return false;
			}
			V = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.f);
			if (A->Num() < 3)
			{
				// Only x, y (map click): ground height.
				FHitResult Hit;
				const FVector Top(V.X, V.Y, 200000.f);
				V.Z = W->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 400000.f), ECC_Visibility) ? Hit.ImpactPoint.Z + 100.f
					: (Who ? Who->GetActorLocation().Z : 0.f);
			}
			return true;
		};

		// ---- world
		if (Op == TEXT("slowmo"))
		{
			UGameplayStatics::SetGlobalTimeDilation(W, FMath::Clamp(float(In->GetNumberField(TEXT("value"))), 0.05f, 4.f));
			return FString();
		}
		if (Op == TEXT("bot_spawn"))
		{
			const AActor* Near = Who ? Who : nullptr;
			FVector At = FVector::ZeroVector;
			if (!Near)
			{
				AGameModeBase* GM = W->GetAuthGameMode();
				const AActor* Start = GM ? GM->FindPlayerStart(nullptr) : nullptr;
				At = Start ? Start->GetActorLocation() : FVector::ZeroVector;
			}
			else
			{
				At = Near->GetActorLocation();
			}
			const int32 Count = FMath::Clamp(int32(In->GetNumberField(TEXT("count"))), 1, 50);
			FString Mode;
			In->TryGetStringField(TEXT("mode"), Mode);
			TArray<TSharedPtr<FJsonValue>> Ids;
			for (int32 i = 0; i < Count; ++i)
			{
				if (ANWCharacter* Bot = Admin ? Admin->SpawnBot(At, In->GetStringField(TEXT("name"))) : nullptr)
				{
					Ids.Add(MakeShared<FJsonValueNumber>(Bot->GetUniqueID()));
					UNWAdminSubsystem::FBot* B = Admin->FindBot(Bot);
					FNWDrive::EMode M;
					if (B && NWAdmin::ParseMode(Mode, M))
					{
						B->Drive.Mode = M;
						B->Drive.Target = Other ? Other : Who;
					}
				}
			}
			Out->SetArrayField(TEXT("ids"), Ids);
			return Ids.Num() ? FString() : TEXT("could not spawn");
		}
		if (Op == TEXT("creature_spawn"))
		{
			const FName Def(*In->GetStringField(TEXT("creature")));
			const FNWCreatureDef* D = ANWCreature::FindDef(Def);
			if (!D || !Who)
			{
				return TEXT("unknown creature or reference");
			}
			const int32 Count = FMath::Clamp(int32(In->GetNumberField(TEXT("count"))), 1, 30);
			const float Dist = FMath::Clamp(float(In->GetNumberField(TEXT("distance"))), 2.f, 60.f) * 100.f;
			bool bAggro = false;
			In->TryGetBoolField(TEXT("aggro"), bAggro);
			TArray<TSharedPtr<FJsonValue>> Ids;
			for (int32 i = 0; i < Count; ++i)
			{
				const float Yaw = Who->GetActorRotation().Yaw + (Count > 1 ? (i - (Count - 1) * 0.5f) * (240.f / Count) : 0.f);
				const FVector At = Who->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * Dist + FVector(0.f, 0.f, 80.f);
				const FTransform Where(FRotator(0.f, Yaw + 180.f, 0.f), At);
				ANWCreature* C = W->SpawnActorDeferred<ANWCreature>(ANWCreature::StaticClass(), Where, nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
				if (!C)
				{
					continue;
				}
				C->Tags.Add(DevTag);
				C->Setup(*D, {}, {});
				C->AggroRange = 3000.f;
				C->FinishSpawning(Where);
				if (bAggro && C->Behavior && C->Behavior->IsActive())
				{
					C->Behavior->AdminSetTarget(Other ? Other : Who);
				}
				Ids.Add(MakeShared<FJsonValueNumber>(C->GetUniqueID()));
			}
			Out->SetArrayField(TEXT("ids"), Ids);
			return FString();
		}
		if (Op == TEXT("remove_all"))
		{
			// Bots and everything summoned (admin or F10); players and the map's spawners stay.
			for (TActorIterator<ANWCharacter> It(W); It; ++It)
			{
				if (It->ActorHasTag(DevTag) || It->ActorHasTag(BotTag))
				{
					It->Destroy();
				}
			}
			return FString();
		}

		// ---- one character
		if (!Who)
		{
			return TEXT("unknown id");
		}
		if (Op == TEXT("dev"))
		{
			if (!PC)
			{
				return TEXT("F10 commands need a player");
			}
			const FString Name = In->GetStringField(TEXT("command"));
			const int64 V = StaticEnum<ENWDevCommand>()->GetValueByNameString(Name);
			if (V != INDEX_NONE)
			{
				PC->RunDev(ENWDevCommand(V));
			}
			else if (FNWDevRegistry::Find(FName(*Name)))
			{
				PC->RunDevNamed(FName(*Name));
			}
			else
			{
				return TEXT("unknown command");
			}
			return FString();
		}
		if (Op == TEXT("coins"))
		{
			if (!Who->Inventory)
			{
				return TEXT("no inventory");
			}
			// Cents; negative takes away. The coin cap still applies (nw-core also enforces it).
			Who->Inventory->AddCoins(int64(In->GetNumberField(TEXT("amount"))));
			Save(PC, Op);
			return FString();
		}
		if (Op == TEXT("level"))
		{
			if (!Who->Progression)
			{
				return TEXT("no progression");
			}
			const int32 To = FMath::Clamp(int32(In->GetNumberField(TEXT("level"))), 1, 200);
			for (int32 Guard = 0; Guard < 200 && Who->Progression->Level < To; ++Guard)
			{
				Who->Progression->AddXp(FMath::Max<int64>(1, UNWProgressionComponent::XpForLevel(Who->Progression->Level + 1) - Who->Progression->Xp));
			}
			Save(PC, Op);
			return FString();
		}
		if (Op == TEXT("bag"))
		{
			if (!Who->Inventory)
			{
				return TEXT("no inventory");
			}
			Who->Inventory->bDevBag = In->GetBoolField(TEXT("on"));
			Who->Inventory->RefreshMaxWeight();
			Who->Inventory->Changed();
			return FString();
		}
		if (Op == TEXT("heal")) { Who->Combat->HealFull(); return FString(); }
		if (Op == TEXT("kill")) { Who->Combat->Kill(); return FString(); }
		if (Op == TEXT("god")) { Who->Combat->bGodMode = In->GetBoolField(TEXT("on")); return FString(); }
		if (Op == TEXT("pause"))
		{
			if (PC)
			{
				return TEXT("players cannot be paused");
			}
			Who->CustomTimeDilation = In->GetBoolField(TEXT("on")) ? 0.f : 1.f;
			return FString();
		}
		if (Op == TEXT("stats"))
		{
			double V = 0.0;
			if (In->TryGetNumberField(TEXT("max_hp"), V) && V > 0.0)
			{
				Who->Combat->MaxHealth = float(V);
				Who->Combat->Health = FMath::Min(Who->Combat->Health, float(V));
			}
			if (In->TryGetNumberField(TEXT("damage"), V) && V >= 0.0) Who->Combat->WeaponBaseDamage = float(V);
			if (In->TryGetNumberField(TEXT("speed"), V) && V > 0.0) Who->DevSpeedMultiplier = FMath::Clamp(float(V), 0.1f, 6.f);
			if (In->TryGetNumberField(TEXT("aggro_range"), V) && V >= 0.0) Who->AggroRange = float(V) * 100.f;
			return FString();
		}
		if (Op == TEXT("teleport"))
		{
			FVector To;
			if (Other)
			{
				To = PointNear(Other, 250.f);
			}
			else if (!ReadVec(TEXT("to"), To))
			{
				return TEXT("missing destination");
			}
			Who->TeleportTo(To, Who->GetActorRotation());
			return FString();
		}
		if (Op == TEXT("remove"))
		{
			if (PC)
			{
				return TEXT("players are not removed");
			}
			Admin && Admin->FindBot(Who) ? Admin->RemoveBot(Who) : (void)Who->Destroy();
			return FString();
		}
		if (Op == TEXT("drive") || Op == TEXT("act"))
		{
			FNWDrive::EMode Mode = FNWDrive::EMode::None;
			FVector Goal = FVector::ZeroVector;
			if (Op == TEXT("drive"))
			{
				if (!NWAdmin::ParseMode(In->GetStringField(TEXT("mode")), Mode))
				{
					return TEXT("unknown mode");
				}
				if (Mode == FNWDrive::EMode::MoveTo && !ReadVec(TEXT("to"), Goal) && Other)
				{
					Goal = Other->GetActorLocation();
				}
				if ((Mode == FNWDrive::EMode::Follow || Mode == FNWDrive::EMode::Fight) && !Other)
				{
					return TEXT("missing target");
				}
			}
			const FName Action(*In->GetStringField(TEXT("action")));
			if (PC)
			{
				// The player's client moves its own character (movement belongs to the client).
				if (Op == TEXT("drive"))
				{
					PC->ClientAdminDrive(uint8(Mode), Goal, Other);
					PC->AdminDriveShown = NWAdmin::ModeName(Mode);
				}
				else
				{
					PC->ClientAdminAct(Action);
				}
				return FString();
			}
			if (Op == TEXT("act"))
			{
				return NWAdmin::Act(Who, Action) ? FString() : TEXT("unknown action");
			}
			UNWAdminSubsystem::FBot* B = Admin ? Admin->FindBot(Who) : nullptr;
			if (!B)
			{
				return TEXT("only players and bots are driven");
			}
			B->Drive.Mode = Mode;
			B->Drive.Goal = Goal;
			B->Drive.Target = Other;
			B->Drive.Home = Who->GetActorLocation();
			return FString();
		}
		if (Op == TEXT("weapon"))
		{
			if (PC)
			{
				return TEXT("players use the weapons equipped in the bag");
			}
			const FString Want = In->GetStringField(TEXT("weapon"));
			const int32 I = Who->WeaponSets.IndexOfByPredicate([&](const FNWWeaponSet& S) { return S.Id.ToString() == Want; });
			if (I == INDEX_NONE)
			{
				return TEXT("unknown weapon");
			}
			const int32 Second = Who->Loadout.IsValidIndex(0) && Who->Loadout[0] != I ? Who->Loadout[0] : (I == 0 ? 1 : 0);
			Who->SetLoadout(I, Second);
			return Who->ActiveWeapon == I ? FString() : TEXT("busy, try again");
		}

		// ---- AI
		UNWBehaviorTreeComponent* BT = Who->FindComponentByClass<UNWBehaviorTreeComponent>();
		if (BT && !BT->IsActive())
		{
			BT = nullptr;
		}
		if (Op == TEXT("ai"))
		{
			if (BT)
			{
				Out->SetObjectField(TEXT("ai"), BT->AdminDescribe());
			}
			return FString();
		}
		if (Op == TEXT("ai_target"))
		{
			if (BT)
			{
				BT->AdminSetTarget(Other);
				return FString();
			}
			if (Who->Brain && Who->Brain->IsActive())
			{
				// The arena brain picks the nearest player-like character inside AggroRange: send it to the target.
				Who->AggroRange = Other ? FMath::Max(Who->AggroRange, float(FVector::Dist2D(Who->GetActorLocation(), Other->GetActorLocation())) + 500.f) : 0.f;
				return FString();
			}
			return TEXT("no AI");
		}
		if (Op == TEXT("ai_freeze")) { if (!BT) return TEXT("no tree"); BT->AdminFreeze(In->GetBoolField(TEXT("on"))); return FString(); }
		if (Op == TEXT("ai_passive")) { if (!BT) return TEXT("no tree"); BT->AdminPassive(In->GetBoolField(TEXT("on"))); return FString(); }
		if (Op == TEXT("ai_replan")) { if (!BT) return TEXT("no tree"); BT->AdminReplan(); return FString(); }
		if (Op == TEXT("ai_vision")) { if (!BT) return TEXT("no tree"); BT->AdminVision(float(In->GetNumberField(TEXT("value")))); return FString(); }
		if (Op == TEXT("ai_bb"))
		{
			if (!BT)
			{
				return TEXT("no tree");
			}
			double V = 0.0;
			BT->AdminOverride(FName(*In->GetStringField(TEXT("key"))), In->TryGetNumberField(TEXT("value"), V) ? V : NAN);
			return FString();
		}
		if (Op == TEXT("brain"))
		{
			if (!Who->Brain || !Who->Brain->IsActive())
			{
				return TEXT("no brain");
			}
			FString Style;
			if (In->TryGetStringField(TEXT("style"), Style))
			{
				const int64 S = StaticEnum<ENWBrainStyle>()->GetValueByNameString(Style);
				if (S != INDEX_NONE)
				{
					Who->Brain->ApplyStyle(ENWBrainStyle(S));
				}
			}
			double V = 0.0;
			if (In->TryGetNumberField(TEXT("block_chance"), V)) Who->Brain->BlockChance = FMath::Clamp(float(V), 0.f, 1.f);
			if (In->TryGetNumberField(TEXT("dodge_chance"), V)) Who->Brain->DodgeChance = FMath::Clamp(float(V), 0.f, 1.f);
			if (In->TryGetNumberField(TEXT("reaction"), V)) Who->Brain->ReactionTime = FMath::Clamp(float(V), 0.f, 3.f);
			if (In->TryGetNumberField(TEXT("hold_distance"), V)) Who->Brain->HoldDistance = FMath::Max(0.f, float(V));
			return FString();
		}
		return TEXT("unknown op");
	}

	TUniquePtr<FHttpServerResponse> Json(const FString& Body, EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok)
	{
		TUniquePtr<FHttpServerResponse> R = FHttpServerResponse::Create(Body, TEXT("application/json"));
		R->Code = Code;
		R->Headers.Add(TEXT("Cache-Control"), {TEXT("no-store")});
		// The page of one server reads the others (8780-8782); untrusted origins never get here (Trusted).
		R->Headers.Add(TEXT("Access-Control-Allow-Origin"), {TEXT("*")});
		return R;
	}

	/** Local only: the listener is on 127.0.0.1, and a page from another site (Origin) or a rebound name (Host) is refused. */
	bool Trusted(const FHttpServerRequest& Req)
	{
		auto LocalHost = [](FString V)
		{
			V.RemoveFromStart(TEXT("http://"));
			const FString HostOnly = V.Left(V.Find(TEXT(":")) == INDEX_NONE ? V.Len() : V.Find(TEXT(":")));
			return HostOnly == TEXT("127.0.0.1") || HostOnly == TEXT("localhost");
		};
		const TArray<FString>* Host = Req.Headers.Find(TEXT("host"));
		const TArray<FString>* Origin = Req.Headers.Find(TEXT("origin"));
		return (!Host || Host->Num() == 0 || LocalHost((*Host)[0])) && (!Origin || Origin->Num() == 0 || LocalHost((*Origin)[0]));
	}

	TSharedPtr<IHttpRouter> GRouter;
	int32 GPort = 0;
}

bool NWAdmin::Start(UWorld* World, int32 Port)
{
	if (Port <= 0)
	{
		Port = 8780 + FMath::Max(0, (World ? World->URL.Port : 7777) - 7777);
	}
	if (GRouter)
	{
		UE_LOG(LogNWAdmin, Display, TEXT("Admin page already on http://127.0.0.1:%d/"), GPort);
		return true;
	}
	TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Port, /*bFailOnBindFailure*/ true);
	if (!Router)
	{
		UE_LOG(LogNWAdmin, Error, TEXT("Admin: port %d is taken"), Port);
		return false;
	}
	Router->RegisterRequestPreprocessor(FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Req, const FHttpResultCallback& Done)
	{
		if (!Trusted(Req))
		{
			Done(Json(TEXT("{\"error\":\"local only\"}"), EHttpServerResponseCodes::Forbidden));
			return true;
		}
		// The page lives at "/", which the router does not accept as a route; it is served here.
		const FString Path = Req.RelativePath.GetPath();
		if (Req.Verb != EHttpServerRequestVerbs::VERB_GET || (Path != TEXT("/") && Path != TEXT("/index.html")))
		{
			return false;   // next handler
		}
		// Read on every request: editing Data/Admin/index.html needs only a reload.
		FString Page;
		if (!FFileHelper::LoadFileToString(Page, *(FPaths::ProjectDir() / TEXT("Data/Admin/index.html"))))
		{
			Page = TEXT("<p>Data/Admin/index.html is missing.</p>");
		}
		TUniquePtr<FHttpServerResponse> R = FHttpServerResponse::Create(Page, TEXT("text/html; charset=utf-8"));
		R->Headers.Add(TEXT("Cache-Control"), {TEXT("no-store")});
		Done(MoveTemp(R));
		return true;
	}));
	Router->BindRoute(FHttpPath(TEXT("/api/state")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateLambda([](const FHttpServerRequest&, const FHttpResultCallback& Done)
	{
		UWorld* W = ServerWorld();
		Done(W ? Json(ToJson(State(W))) : Json(TEXT("{\"error\":\"no world\"}"), EHttpServerResponseCodes::ServiceUnavail));
		return true;
	}));
	Router->BindRoute(FHttpPath(TEXT("/api/catalog")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateLambda([](const FHttpServerRequest&, const FHttpResultCallback& Done)
	{
		Done(Json(ToJson(Catalog())));
		return true;
	}));
	Router->BindRoute(FHttpPath(TEXT("/api/cmd")), EHttpServerRequestVerbs::VERB_POST, FHttpRequestHandler::CreateLambda([](const FHttpServerRequest& Req, const FHttpResultCallback& Done)
	{
		UWorld* W = ServerWorld();
		FUTF8ToTCHAR Body(reinterpret_cast<const ANSICHAR*>(Req.Body.GetData()), Req.Body.Num());
		TSharedPtr<FJsonObject> In;
		if (!W || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Body.Length(), Body.Get())), In) || !In.IsValid())
		{
			Done(Json(TEXT("{\"error\":\"bad request\"}"), EHttpServerResponseCodes::BadRequest));
			return true;
		}
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		const FString Error = Command(W, In, Out);
		if (!Error.IsEmpty())
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		Done(Json(ToJson(Out), Error.IsEmpty() ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest));
		return true;
	}));
	FHttpServerModule::Get().StartAllListeners();
	GRouter = Router;
	GPort = Port;
	UE_LOG(LogNWAdmin, Display, TEXT("Admin page on http://127.0.0.1:%d/"), Port);
	return true;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs AdminCmd(TEXT("nw.admin"),
		TEXT("Admin page and API on 127.0.0.1: nw.admin [port] (default 8780 + game port - 7777). docs/tech/admin.md"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			NWAdmin::Start(ServerWorld(), Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
		}));

	/** -nwadmin / -nwadmin=<port> on the server command line: starts with the first server map. */
	struct FNWAdminAuto
	{
		FNWAdminAuto()
		{
			FWorldDelegates::OnWorldInitializedActors.AddLambda([](const FActorsInitializedParams& P)
			{
				UWorld* W = P.World;
				if (!W || W->GetNetMode() == NM_Client || !(W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE))
				{
					return;
				}
				int32 Port = 0;
				if (FParse::Value(FCommandLine::Get(), TEXT("-nwadmin="), Port) || FParse::Param(FCommandLine::Get(), TEXT("nwadmin")))
				{
					NWAdmin::Start(W, Port);
				}
			});
		}
	};
	FNWAdminAuto AdminAuto;
}
#else
bool NWAdmin::Start(UWorld*, int32)
{
	return false;
}
#endif
