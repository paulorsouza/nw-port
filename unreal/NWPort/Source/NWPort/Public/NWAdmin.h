#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NWAdmin.generated.h"

class ANWCharacter;

/**
 * Admin tool for demos and tests (docs/tech/admin.md): a web page served by the game server itself, with an HTTP API
 * on 127.0.0.1 only. Lists who is on the server, creates bots, drives the bots and the players' own characters, runs
 * the F10 commands on anyone, gives coins and levels, summons creatures and steers their AI.
 *
 * Off by default. `-nwadmin` on the server command line turns it on at 8780 + (game port - 7777) (Windsward 8780,
 * Everfall 8781, Arena 8782); `-nwadmin=<port>` picks the port; the console command `nw.admin <port>` turns it on in a
 * running server or in Play in the editor. Not in Shipping builds.
 */

/** What a driven character is doing: bots on the server, a player's character on its own client. */
struct NWPORT_API FNWDrive
{
	enum class EMode : uint8 { None, MoveTo, Follow, Fight, Wander };
	EMode Mode = EMode::None;
	FVector Goal = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Target;
	double NextAttack = 0.0;
	double NextGoal = 0.0;
	FVector Home = FVector::ZeroVector;
};

namespace NWAdmin
{
	/** One step of the drive (movement input, facing and attacks). Runs where the character's movement is owned. */
	NWPORT_API void TickDrive(ANWCharacter* Me, FNWDrive& D, float DeltaSeconds);
	/** One-shot action: light, heavy, ability1..3, dodge, jump, block, unblock, swap. False if unknown. */
	NWPORT_API bool Act(ANWCharacter* Me, FName Action);
	NWPORT_API FString ModeName(FNWDrive::EMode Mode);
	NWPORT_API bool ParseMode(const FString& Name, FNWDrive::EMode& Out);
	/** Starts the HTTP API and the page on Port (0 = 8780 + game port - 7777). False if the port is taken. */
	NWPORT_API bool Start(UWorld* World, int32 Port);
}

/** Bots of the admin tool: server characters without a connection, marked as players, driven by the API. */
UCLASS()
class NWPORT_API UNWAdminSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UNWAdminSubsystem* Get(const UWorld* World);

	struct FBot
	{
		TWeakObjectPtr<ANWCharacter> Pawn;
		FString Name;
		FNWDrive Drive;
	};

	ANWCharacter* SpawnBot(const FVector& Near, const FString& Name);
	FBot* FindBot(const AActor* Pawn);
	const TArray<FBot>& Bots() const { return BotList; }
	void RemoveBot(const AActor* Pawn);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UNWAdminSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<FBot> BotList;
	int32 NextBotNumber = 1;
};
