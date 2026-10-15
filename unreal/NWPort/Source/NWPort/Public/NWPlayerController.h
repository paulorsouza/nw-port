#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NWBackend.h"
#include "NWPlayerController.generated.h"

class SWidget;

// Dev menu commands (run on the server).
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
	GiveCraftLevels,
	GiveCraftMaterials,
	GoToCreature,
	ToggleFly,
	// Test arena: waves around the player (UNWBrainComponent).
	ArenaBoars,
	ArenaWolves,
	ArenaLost,
	ArenaDuelist,
	ArenaMixed,
	ArenaArcher,
	ArenaRapier,
	TravelEverfall,
};

/** Chat channels (the game's names: ui_chat_name_local / _group, ui_error_systemname). */
UENUM()
enum class ENWChatChannel : uint8
{
	Area,
	Group,
	System,
};

/**
 * Player menus: Weapon Mastery (K, game screen drawn by the HUD) and dev (F10, Slate). While a menu is
 * open, the mouse shows and the camera stops.
 */
UCLASS()
class NWPORT_API ANWPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANWPlayerController();
	virtual void BeginPlay() override;
	// Development (outside Shipping): reapplies the saved F10 options (NWDevPrefs) to the possessed character.
	virtual void OnPossess(APawn* InPawn) override;
	/** Esc: closes the open menu; with no menu, opens the game menu (Continue, Settings, Quit). */
	void OnEscape();

	// Windsward music and ambience (local player only).
	UPROPERTY(VisibleAnywhere, Category = "NW") TObjectPtr<class UNWAmbienceComponent> Ambience;

	void ToggleSkillMenu();
	void ToggleInventory();
	/** World map (M). */
	void ToggleMap();
	void ToggleDevMenu();
	void CloseMenus();
	/** Slate screen on top of the game (F10 menu, Trading Post): closes the others, focus on Focus; Esc closes. */
	void ShowMenu(TSharedRef<SWidget> Menu, TSharedRef<SWidget> Focus);
	bool IsMenuOpen() const;
	/** Enters menu mode for a screen drawn on the canvas (clicks through OnMenuClick), e.g. area 4 screens. */
	void EnterCanvasMenu();
	/** Frame in which a click was a menu click (the controller processes before the character: a click that closes the
	 *  screen cannot become an attack in the same frame). ANWCharacter::InMenu checks it. */
	uint64 MenuClickFrame = 0;
	bool IsInventoryOpen() const;

	/**
	 * Test commands (F10): the server only accepts them from whoever is allowed (admin account in nw-core; without nw-core or
	 * in the editor's Play, everyone). Each use goes to the server log and to the item log ("dev:<command>").
	 */
	UPROPERTY(Replicated) bool bDevAllowed = false;
	/** Server: recomputes bDevAllowed (at the start and when the character arrives from nw-core). */
	void UpdateDevAllowed();
	UFUNCTION(Server, Reliable) void ServerDev(ENWDevCommand Command);
	/** F10 button registered by an area (NW_REGISTER_DEV, NWDevRegistry.h). */
	UFUNCTION(Server, Reliable) void ServerDevNamed(FName Id);

	// Chat: the server validates, limits and distributes; a leading "/" is a command.
	UFUNCTION(Server, Reliable) void ServerSendChat(const FString& Text);
	/**
	 * Unstuck (Esc menu), like the game: goes back to a recent position where the character was on the ground
	 * ("breadcrumbs" stored by the server every second), after 5 s; 60 s cooldown; not available while dead.
	 */
	UFUNCTION(Server, Reliable) void ServerUnstuck();
	UFUNCTION(Client, Reliable) void ClientChat(ENWChatChannel Channel, const FString& Sender, const FString& Text);
	void OpenChat();
	bool IsChatOpen() const;
	// Server: System message for this player only.
	void SystemMessage(const FString& Text) { ClientChat(ENWChatChannel::System, FString(), Text); }
	/** Notice on the game's warning bar (lyshineui/warningbar), not in chat: bag full, action refused... (#key|args). */
	void Warning(const FString& Text) { ClientWarning(Text); }
	/** Server: takes this player to Map (path /Game/NW/Maps/...): alone, the local world changes; with one server per
	 *  map, only this player goes to the other server; a server without nw-core takes everyone along. */
	void TravelToMap(const FString& Map);
	UFUNCTION(Client, Reliable) void ClientWarning(const FString& Text);
	// Local command (client debug cvars).
	void LocalConsole(const FString& Command);

	// Server with nw-core: character loaded from the backend (empty without backend).
	TOptional<FNWCharacterRecord> Record;
	bool bPlacedFromRecord = false;
	/** Server: map this player is travelling to (ANWGameMode::TravelPlayer); saves from then on write it, without position. */
	FString TravelMap;
	// Server: sends saved abilities and Weapon Mastery to the owning client.
	void PublishProgress();
	// Client → server: passives marked on the Weapon Mastery screen (key = weapon from abilities.json).
	UFUNCTION(Server, Reliable) void ServerSetMastery(const FString& Weapon, const TArray<FString>& Ids);

protected:
	// Player leaving: saves before Unreal destroys the character (the game mode's Logout comes later).
	virtual void PawnLeavingGame() override;
	// Load test: -nwbot walks in random directions and attacks/uses abilities/dodges from time to time.
	virtual void PlayerTick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;
	/** Shortcuts tab waiting for a key: the next key goes to the action (Esc cancels) and does not reach the game. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	/** After changing keys: rebuilds the character's mapping. */
	void ApplyKeyChanges();

	// {"loadout": {...}, "mastery": {...}}; the client writes to GameUserSettings.ini, which the screens already read.
	UPROPERTY(ReplicatedUsing = OnRep_Progress) FString ProgressJson;
	UFUNCTION() void OnRep_Progress();

private:
	void EnterMenuInput();
	void LeaveMenuInput();
	void OnMenuClick();
	void OnMenuRightClick();
	void OnWheelUp();
	void OnWheelDown();
	class ANWHUD* GetNWHUD() const;

	void HandleChatCommand(const FString& Text);
	TArray<double> ChatTimes;   // server: message limit

	int32 BotMode = -1;   // -1 = not read, 0 = off, 1 = bot
	float BotTimer = 0.f;
	float BotReleaseIn = 0.f;
	// Tests: -nwtestdie=N dies N s after joining; -nwtestrespawn clicks Respawn when allowed.
	float TestDieIn = -1.f;
	bool bTestRespawn = false;
	FString TestChat;          // -nwtestchat="a|b": sends each message 1.5 s apart
	float TestChatIn = 3.f;
	FString TestDev;           // -nwtestdev="SpawnLost|KillEnemies": dev commands, one every 2 s
	float TestDevIn = 3.f;
	FString TestInv;           // -nwtestinv="potion|move:3:12|unequip:1|equip:12:0": inventory actions, 1.5 s each
	float TestInvIn = 4.f;
	int32 TestGather = 0;      // -nwtestgather=N: presses E N times on the focused gatherable (waits for each gathering to finish)
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
