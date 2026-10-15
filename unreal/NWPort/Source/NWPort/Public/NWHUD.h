#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NWUILayout.h"
#include "NWMasteryMenu.h"
#include "NWLoginMenu.h"
#include "NWRespawnScreen.h"
#include "NWChat.h"
#include "NWInventoryMenu.h"
#include "NWWorldMap.h"
#include "NWTradeskillMenu.h"
#include "NWRefiningMenu.h"
#include "NWShopMenu.h"
#include "NWAttributesMenu.h"
#include "NWGameMenu.h"
#include "NWHUD.generated.h"

class UFont;
class UNWCombatComponent;
class UTexture2D;

/**
 * Combat HUD with the game's screens (hud2024: vitals, weaponquickslots, consumablequickslots, reticles),
 * drawn by FNWUILayout. Only the values the game's Lua set at runtime go here:
 * bar fill, ability icons and cooldowns, key hints.
 */
UCLASS()
class NWPORT_API ANWHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "NW") float TargetRange = 1200.f;
	// Movement/animation diagnostic line in the corner (nw.HudDebug also enables it).
	UPROPERTY(EditAnywhere, Category = "NW") bool bShowDiagnostics = false;

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	// Weapon Mastery screen (K): drawn here, clicks forwarded by the controller.
	FNWMasteryMenu Mastery;
	// Clan entry screen (client with nw-core, before connecting).
	FNWLoginMenu Login;
	// Death screen (player dead until clicking Respawn).
	FNWRespawnScreen Respawn;
	FNWChat Chat;
	FNWInventoryMenu Inventory;
	FNWWorldMap WorldMap;
	FNWTradeskillMenu Tradeskills;
	FNWAttributesMenu Attributes;
	// Area 4 screens opened from the world (refining station; later Storage, Trading Post, houses). The
	// ANWPlayerController only asks through these four functions, without knowing each screen.
	FNWRefiningMenu Refining;
	FNWShopMenu Shop;
	void OpenMarket(class ANWCharacter* Who, class ANWStation* Station);
	/** City boards (governance): town project (players) or territory planning (governor). */
	void OpenCity(class ANWCharacter* Who, class ANWStation* Station, bool bGovernance);
	void OpenRefining(class ANWCharacter* Who, class ANWStation* Station);
	void OpenStorage(class ANWCharacter* Who, class ANWStation* Station);
	/** Bag on the ground: inventory with the "Dropped Items" window (catcontainer in loot bag mode). */
	void OpenLootBag(class ANWCharacter* Who, class ANWLootBag* Bag);
	void OpenShop(class ANWCharacter* Who, const FString& ShopId, const FString& Title);
	bool IsStationMenuOpen() const { return Refining.IsOpen() || Shop.IsOpen(); }
	void CloseStationMenus() { Refining.Close(); Shop.Close(); }
	bool StationMenuClick(const FVector2D& Mouse) { return (Refining.IsOpen() && Refining.Click(Mouse)) || (Shop.IsOpen() && Shop.Click(Mouse)); }
	bool StationMenuWheel(float Delta);
	/** Damage numbers (lyshineui/hud/damagenumbers): rise and fade over the target in 1.2 s. */
	void AddDamageNumber(const FVector& World, const FString& Text, const FLinearColor& Color, bool bBig);
	/** Hit received (lyshineui/hitindicator): arrow around the reticle and red glow on the edge on the attacker's side. */
	void AddHitIndicator(const FVector& From, float Fraction);
	// Esc menu and settings (drawn on top of everything).
	FNWGameMenu GameMenu;
	bool IsDeathScreen() const { return bDeathScreen; }
	/** Loot received (UNWInventoryComponent::ClientLoot): one line per item, on the right, for 6 s. */
	void ShowLoot(const TArray<struct FNWItem>& Gained, int64 Coins);
	/** Gathering trade skill XP: progress line in the loot ticker (trade skill icon, "+N", name and level bar). */
	void ShowTradeXp(uint8 Skill, int32 Amount);
	/** XP gained (UNWProgressionComponent::ClientXpGained): "+N XP" next to the level for 2.5 s. */
	void ShowXp(int64 Amount);
	/** The game's warning bar (lyshineui/warningbar) for 3 s. */
	void ShowWarning(const FString& Text) { WarningText = Text; WarningAt = FPlatformTime::Seconds(); }
	// After changing the language (NWText::SetLang): reloads the screens.
	void ReloadLanguage();
	/** Offline capture (NWUIShotCommandlet, tools/ui_shots/hud.json): loads the layouts and draws Who's combat HUD
	 *  on InCanvas, with the camera at CameraYaw, without a player controller. */
	void DrawShot(UCanvas* InCanvas, const class ANWCharacter* Who, float CameraYaw, bool bInCombat, const FString& MapName);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool bDeathScreen = false;
	bool bMenuOpened = false;      // -nwmenu test
	double HudFirstDraw = 0.0;
	void SetDeathScreen(bool bOn);
	void InitLayouts();
	void SetupStatic();
	/** The combat HUD (layout, compass, tickers, cards) when no full screen is open. */
	void DrawCombatHUD(const class ANWCharacter* Me, float Dt);
	// "In combat" as the vitals see it (healthmeter.lua isInCombat): damage taken or stamina spent in the last 10 s.
	double CombatUntil = 0.0;
	float LastHealthSeen = -1.f;
	float LastStaminaSeen = -1.f;
	// Offline capture: pawn and camera yaw without a player controller.
	const class ANWCharacter* ShotPawn = nullptr;
	float ShotYaw = 0.f;
	bool bShotCombat = false;
	FString ShotMap;
	float ViewYaw() const;
	void UpdateVitals(const UNWCombatComponent* C, float DeltaSeconds);
	void UpdateAbilities(const class ANWCharacter* Me, float DeltaSeconds);
	TMap<FName, TPair<FString, FString>> AbilityArt;   // action -> (icon, background)
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);
	void DrawTarget(const APawn* Me);
	void DrawNameplates(const APawn* Me);
	void UpdateConsumables(const class ANWCharacter* Me);
	/** The game's XP bar (hud2024/xp): level in the middle, fill of the current level. */
	void UpdateXp(const class ANWCharacter* Me);
	int64 XpShown = 0;
	double XpShownAt = -100.0;
	/** Level-up banner (lyshineui/banner/bannerlevelup, XP part): rune, ribbon, "Level Up" and the level. */
	void DrawLevelUp();
	int32 LastLevel = 0;
	int32 LevelUpValue = 0;
	double LevelUpAt = -100.0;
	void DrawLootTicker();
	/**
	 * The game's compass at the top (lyshineui/compass): compass2048 strip scrolling with the camera, compassbg line and needle.
	 * Map north = yaw −4° in L_SouthWindsward (city rotation −94°: atan2(cos a, −sin a)).
	 */
	void DrawCompass();
	void DrawDamageNumbers();
	void DrawHitIndicators();
	void DrawWarning();
public:
	/**
	 * The game's navigation bar (lyshineui/navbar, 2023) at the top of full screens: Inventory, Character and
	 * Map, with the free points badge on Character. Active = 0, 1, 2. NavTabAt returns the tab under the mouse.
	 */
	void DrawNavbar(int32 Active, const FVector2D& Mouse);
	int32 NavTabAt(const FVector2D& Mouse) const;
private:
	FBox2f NavTabs[3] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};
	FString WarningText;
	double WarningAt = -100.0;
	struct FHitIndicator { FVector From; float Fraction = 0.f; double Time = 0.0; };
	TArray<FHitIndicator> HitIndicators;
	/** Bag on the action bar (hud2024/actionbar/encumbrance): weight %; overloaded turns red with the game's warning. */
	void DrawEncumbrance(const APawn* Me);
	struct FDamageNumber
	{
		FVector World;
		FString Text;
		FLinearColor Color;
		bool bBig = false;
		double Time = 0.0;
		float Drift = 0.f;
	};
	TArray<FDamageNumber> DamageNumbers;
	float CompassNorthYaw = -4.f;
	/** The game's HUD (everything but the Esc menu, which DrawHUD draws on top). */
	void DrawGameHUD();
	/** Gathering: "[E] Mine Iron Vein" near a gatherable and the bar during gathering. */
	void DrawGather(const APawn* Me);
	/** Interact card (NPC, station...: NWInteractable.h) and the game's icon over the NPC with a gift. */
	void DrawInteract(const APawn* Me);
	void DrawNpcNames(const APawn* Me);
	/** Name ribbon to the right of the ring and the [E] action line (or warning in red), from the unifiedinteract card. */
	void DrawCardText(const FVector2D& Ring, const FString& Name, const FString& Action, bool bBlocked);
	// Loot ticker line (hud2024/lootticker): item (icon, rarity, tier, quantity) or coins.
	// Ticker line: item, coins or trade skill XP (Trade = NWTradeskill, Quantity = XP).
	struct FLootLine { FName ItemId; int32 Quantity = 0; uint8 Rarity = 0; int64 Coins = 0; double Time = 0.0; int32 Trade = -1; };
	TArray<FLootLine> LootLines;
	// Group members on the left, with the game's member bar (Data/UI/group_layout.json).
	void DrawGroup(const APawn* Me);
	FNWUILayout GroupLayout;
	static FString DisplayName(const AActor* Actor);
	void DrawDiagnostics(const APawn* Me);
	const UNWCombatComponent* FindTarget(const APawn* Self) const;
	UTexture2D* ResolveTexture(const FString& GameTexture);

	FNWUILayout Layout;
	bool bStaticDone = false;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> Textures;
	UPROPERTY() TArray<TObjectPtr<UFont>> Fonts;

	// "Delayed" bar (UnderFill) that goes down slowly after damage, as in the game.
	float HealthLag = 1.f;
	float StaminaLag = 1.f;
	float LastHealth = -1.f;
	TArray<float> ReadyFlash;   // per slot: remaining time of the "ready" glow
	TArray<bool> WasCooling;
	double LastDrawTime = 0.0;
};
