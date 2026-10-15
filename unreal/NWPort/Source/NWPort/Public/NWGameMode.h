#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NWBackend.h"
#include "NWGameMode.generated.h"

class ANWPlayerController;
class ANWPlayerState;

/**
 * With nw-core enabled (see UNWBackend), a connection only gets in with a valid `?ticket=`: the pass is redeemed in
 * PreLoginAsync, the character spawns where it stopped, and the state is saved every SaveInterval seconds and on exit.
 */
UCLASS()
class NWPORT_API ANWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANWGameMode();

	virtual void PreLoginAsync(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, const FOnPreLoginCompleteDelegate& OnComplete) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Copies the player's current state into the record and sends it to the backend; Reason goes to the item log. */
	void SavePlayer(ANWPlayerController* PC, const FString& Reason = TEXT("save"));

	/** Server with nw-core (pass required on connect). */
	bool IsBackendServer() const;
	/**
	 * One dedicated server per map (docs/tech/server.md): saves the player with Map and no position (spawns at the map's
	 * start), asks nw-core for a new pass and the address of that map's server, and sends the client there.
	 */
	void TravelPlayer(ANWPlayerController* PC, const FString& Map);
	/** F10: another character of the account (editor Play: in place; client with login: back to the character list). */
	void SwitchCharacter(ANWPlayerController* PC);
	/** Puts the character record on the player (name, position on this map, inventory, progression). */
	void ApplyRecord(ANWPlayerController* PC, struct FNWCharacterRecord&& Record);

	/** Periodic save (s): whatever is not an operation written on the spot (position, health, XP; gathering and loot until step 3). */
	UPROPERTY(EditAnywhere, Category = "NW|Servidor") float SaveInterval = 5.f;

	// Groups (up to MaxGroupSize). Chat commands: /convidar /aceitar /recusar /sair /g.
	void GroupInvite(ANWPlayerController* From, const FString& TargetName);
	void GroupAccept(ANWPlayerController* PC);
	void GroupDecline(ANWPlayerController* PC);
	void GroupLeave(ANWPlayerController* PC);
	void GroupChat(ANWPlayerController* From, const FString& Text);
	static constexpr int32 MaxGroupSize = 5;

	/** Loot from a dead enemy: the killer and their group within 60 m, each with their own roll. */
	void GrantLoot(APawn* Killer, const AActor* Victim, FName Table);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UNWBackend* Backend() const;
	bool IsCharacterOnline(const FString& CharacterId) const;
	void SaveAll(const TCHAR* Reason);
	void SavePeriodic() { SaveAll(TEXT("periodic")); }
	/** Editor Play with backend (docs/tech/backend-testing.md): joins with the editor account and applies the saved character. */
	void PieEnter(ANWPlayerController* PC);
	void HandleTearDown(UWorld* World);
	FDelegateHandle TearDownHandle;

	TArray<ANWPlayerState*> GroupMembers(int32 GroupId) const;
	ANWPlayerState* FindPlayer(const FString& Name) const;
	void GroupSystem(int32 GroupId, const FString& Message);
	struct FInvite { TWeakObjectPtr<ANWPlayerState> From; double Expires = 0.0; };
	TMap<TWeakObjectPtr<ANWPlayerState>, FInvite> Invites;   // key = invitee
	int32 NextGroupId = 1;

	/** Redeemed passes waiting for Login (key = pass). */
	TMap<FString, FNWCharacterRecord> Pending;
	FTimerHandle SaveTimer;
};
