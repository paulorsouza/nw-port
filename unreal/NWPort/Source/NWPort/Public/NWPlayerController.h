#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NWBackend.h"
#include "NWPlayerController.generated.h"

class SWidget;

// Comandos do menu de dev (executados no servidor).
UENUM()
enum class ENWDevCommand : uint8
{
	ToggleNoCooldowns,
	ToggleGodMode,
	ToggleInfiniteStamina,
	HealSelf,
	ClearCooldowns,
	SpawnLost,
	SpawnDummy,
	SpawnBlockingDummy,
	SpawnAttackingDummy,
	KillEnemies,
	RemoveSpawned,
	ToggleSlowMotion,
	TravelArena,
	TravelCity,
	KillSelf,
	GiveTools,
	GoToOre,
	ToggleFastRun,
	GiveLevel,
	GiveTradeLevels,
};

/** Canais do chat (nomes do jogo: ui_chat_name_local / _group, ui_error_systemname). */
UENUM()
enum class ENWChatChannel : uint8
{
	Area,
	Group,
	System,
};

/**
 * Menus do jogador: maestria (K, tela do jogo desenhada pela HUD) e dev (F10, Slate). Enquanto um menu está
 * aberto, o mouse aparece e a câmera para.
 */
UCLASS()
class NWPORT_API ANWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANWPlayerController();
	virtual void BeginPlay() override;
	/** Esc: fecha o menu aberto; sem menu, abre o menu do jogo (Continuar, Configurações, Sair). */
	void OnEscape();

	// Música e ambiente de Guardaventos (só no jogador local).
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<class UNWAmbienceComponent> Ambience;

	void ToggleSkillMenu();
	void ToggleInventory();
	/** Mapa do mundo (M). */
	void ToggleMap();
	void ToggleDevMenu();
	void CloseMenus();
	bool IsMenuOpen() const;
	bool IsInventoryOpen() const;

	UFUNCTION(Server, Reliable) void ServerDev(ENWDevCommand Command);

	// Chat: o servidor valida, limita e distribui; "/" no começo é comando.
	UFUNCTION(Server, Reliable) void ServerSendChat(const FString& Text);
	/**
	 * Desprender (menu do Esc), como o jogo: volta para uma posição recente em que o personagem estava no chão
	 * ("migalhas" guardadas pelo servidor a cada segundo), depois de 5 s; recarga de 60 s; não vale morto.
	 */
	UFUNCTION(Server, Reliable) void ServerUnstuck();
	UFUNCTION(Client, Reliable) void ClientChat(ENWChatChannel Channel, const FString& Sender, const FString& Text);
	void OpenChat();
	bool IsChatOpen() const;
	// Servidor: mensagem do Sistema só para este jogador.
	void SystemMessage(const FString& Text) { ClientChat(ENWChatChannel::System, FString(), Text); }
	// Comando local (cvars de depuração do cliente).
	void LocalConsole(const FString& Command);

	// Servidor com nw-core: personagem carregado do backend (vazio sem backend).
	TOptional<FNWCharacterRecord> Record;
	bool bPlacedFromRecord = false;
	// Servidor: manda habilidades e maestria salvas para o cliente dono.
	void PublishProgress();
	// Cliente → servidor: passivas marcadas na tela de maestria (chave = arma do abilities.json).
	UFUNCTION(Server, Reliable) void ServerSetMastery(const FString& Weapon, const TArray<FString>& Ids);

protected:
	// Saída do jogador: salva antes de a Unreal destruir o personagem (o Logout do game mode vem depois).
	virtual void PawnLeavingGame() override;
	// Teste de carga: -nwbot anda em direções aleatórias e ataca/usa habilidades/esquiva de tempos em tempos.
	virtual void PlayerTick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	/** Aba Atalhos esperando tecla: a próxima tecla vai para a ação (Esc cancela) e não chega ao jogo. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	/** Depois de trocar teclas: refaz o mapeamento do personagem. */
	void ApplyKeyChanges();

	// {"loadout": {...}, "mastery": {...}}; o cliente grava no GameUserSettings.ini, que as telas já leem.
	UPROPERTY(ReplicatedUsing = OnRep_Progress) FString ProgressJson;
	UFUNCTION() void OnRep_Progress();

private:
	void ShowMenu(TSharedRef<SWidget> Menu, TSharedRef<SWidget> Focus);
	void EnterMenuInput();
	void LeaveMenuInput();
	void OnMenuClick();
	void OnMenuRightClick();
	void OnWheelUp();
	void OnWheelDown();
	class ANWHUD* GetNWHUD() const;

	void HandleChatCommand(const FString& Text);
	TArray<double> ChatTimes;   // servidor: limite de mensagens

	int32 BotMode = -1;   // -1 = não lido, 0 = desligado, 1 = bot
	float BotTimer = 0.f;
	float BotReleaseIn = 0.f;
	// Testes: -nwtestdie=N morre N s depois de entrar; -nwtestrespawn clica Renascer quando liberar.
	float TestDieIn = -1.f;
	bool bTestRespawn = false;
	FString TestChat;          // -nwtestchat="a|b": manda cada mensagem com 1,5 s de intervalo
	float TestChatIn = 3.f;
	FString TestDev;           // -nwtestdev="SpawnLost|KillEnemies": comandos de dev, um a cada 2 s
	float TestDevIn = 3.f;
	FString TestInv;           // -nwtestinv="potion|move:3:12|unequip:1|equip:12:0": ações de inventário, 1,5 s cada
	float TestInvIn = 4.f;
	int32 TestGather = 0;      // -nwtestgather=N: aperta E N vezes no coletável em foco (espera cada coleta acabar)
	float TestGatherIn = 6.f;
	FVector BotDir = FVector::ZeroVector;

	TSharedPtr<SWidget> OpenMenu;
	TSharedPtr<SWidget> OpenMenuHost;

private:
	void RecordBreadcrumb();
	void FinishUnstuck();
	TArray<FVector> Breadcrumbs;
	FTimerHandle BreadcrumbTimer;
	FTimerHandle UnstuckTimer;
	double UnstuckReadyAt = 0.0;
};
