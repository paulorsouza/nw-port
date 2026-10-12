#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NWBackend.h"
#include "NWGameMode.generated.h"

class ANWPlayerController;
class ANWPlayerState;

/**
 * Com o nw-core ligado (ver UNWBackend), a conexão só entra com `?ticket=` válido: o passe é resgatado no
 * PreLoginAsync, o personagem nasce onde parou, e o estado é salvo a cada SaveInterval segundos e ao sair.
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

	/** Copia o estado atual do jogador para o registro e manda para o backend. */
	void SavePlayer(ANWPlayerController* PC);

	UPROPERTY(EditAnywhere, Category = "NW|Servidor") float SaveInterval = 30.f;

	// Grupos (até MaxGroupSize). Comandos de chat: /convidar /aceitar /recusar /sair /g.
	void GroupInvite(ANWPlayerController* From, const FString& TargetName);
	void GroupAccept(ANWPlayerController* PC);
	void GroupDecline(ANWPlayerController* PC);
	void GroupLeave(ANWPlayerController* PC);
	void GroupChat(ANWPlayerController* From, const FString& Text);
	static constexpr int32 MaxGroupSize = 5;

	/** Saque de um inimigo morto: quem matou e o grupo dele a até 60 m, cada um com o próprio sorteio. */
	void GrantLoot(APawn* Killer, const AActor* Victim, FName Table);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UNWBackend* Backend() const;
	bool IsCharacterOnline(const FString& CharacterId) const;
	void SaveAll();

	TArray<ANWPlayerState*> GroupMembers(int32 GroupId) const;
	ANWPlayerState* FindPlayer(const FString& Name) const;
	void GroupSystem(int32 GroupId, const FString& Message);
	struct FInvite { TWeakObjectPtr<ANWPlayerState> From; double Expires = 0.0; };
	TMap<TWeakObjectPtr<ANWPlayerState>, FInvite> Invites;   // chave = convidado
	int32 NextGroupId = 1;

	/** Passes resgatados esperando o Login (chave = passe). */
	TMap<FString, FNWCharacterRecord> Pending;
	FTimerHandle SaveTimer;
};
