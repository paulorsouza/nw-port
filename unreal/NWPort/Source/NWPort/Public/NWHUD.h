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
#include "NWStorageMenu.h"
#include "NWAttributesMenu.h"
#include "NWGameMenu.h"
#include "NWHUD.generated.h"

class UFont;
class UNWCombatComponent;
class UTexture2D;

/**
 * HUD de combate com as telas do jogo (hud2024: vitals, weaponquickslots, consumablequickslots, reticles),
 * desenhadas por FNWUILayout. Aqui só entram os valores que o Lua do jogo punha em tempo de execução:
 * preenchimento das barras, ícones e cooldown das habilidades, dicas de tecla.
 */
UCLASS()
class NWPORT_API ANWHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "NW") float TargetRange = 1200.f;
	// Linha de diagnóstico do movimento/animação no canto (nw.HudDebug também liga).
	UPROPERTY(EditAnywhere, Category = "NW") bool bShowDiagnostics = false;

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	// Tela de maestria (K): desenhada aqui, cliques repassados pelo controlador.
	FNWMasteryMenu Mastery;
	// Tela de entrada do clã (cliente com nw-core, antes de conectar).
	FNWLoginMenu Login;
	// Tela de morte (jogador morto até clicar Renascer).
	FNWRespawnScreen Respawn;
	FNWChat Chat;
	FNWInventoryMenu Inventory;
	FNWWorldMap WorldMap;
	FNWTradeskillMenu Tradeskills;
	FNWAttributesMenu Attributes;
	// Telas da esteira 4 abertas a partir do mundo (estação de refino; depois armazém, posto de trocas, casas). O
	// ANWPlayerController só pergunta por estas quatro funções, sem conhecer cada tela.
	FNWRefiningMenu Refining;
	FNWStorageMenu Storage;
	void OpenRefining(class ANWCharacter* Who, class ANWStation* Station);
	void OpenStorage(class ANWCharacter* Who, class ANWStation* Station);
	bool IsStationMenuOpen() const { return Refining.IsOpen() || Storage.IsOpen(); }
	void CloseStationMenus() { Refining.Close(); if (Storage.IsOpen()) { Storage.Close(); } }
	bool StationMenuClick(const FVector2D& Mouse) { return (Refining.IsOpen() && Refining.Click(Mouse)) || (Storage.IsOpen() && Storage.Click(Mouse)); }
	bool StationMenuWheel(float Delta);
	/** Números de dano (lyshineui/hud/damagenumbers): sobem e somem sobre o alvo em 1,2 s. */
	void AddDamageNumber(const FVector& World, const FString& Text, const FLinearColor& Color, bool bBig);
	/** Golpe recebido (lyshineui/hitindicator): seta em volta da mira e brilho vermelho na borda do lado do atacante. */
	void AddHitIndicator(const FVector& From, float Fraction);
	// Menu do Esc e configurações (desenhado por cima de tudo).
	FNWGameMenu GameMenu;
	bool IsDeathScreen() const { return bDeathScreen; }
	/** Saque recebido (UNWInventoryComponent::ClientLoot): uma linha por item, à direita, por 6 s. */
	void ShowLoot(const TArray<struct FNWItem>& Gained, int64 Coins);
	/** XP de ofício da coleta: linha de progresso no aviso de saque (ícone do ofício, "+N", nome e barra do nível). */
	void ShowTradeXp(uint8 Skill, int32 Amount);
	/** XP ganho (UNWProgressionComponent::ClientXpGained): "+N XP" ao lado do nível por 2,5 s. */
	void ShowXp(int64 Amount);
	/** Faixa de aviso do jogo (lyshineui/warningbar) por 3 s. */
	void ShowWarning(const FString& Text) { WarningText = Text; WarningAt = FPlatformTime::Seconds(); }
	// Depois de trocar o idioma (NWText::SetLang): recarrega as telas.
	void ReloadLanguage();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool bDeathScreen = false;
	bool bMenuOpened = false;      // teste -nwmenu
	double HudFirstDraw = 0.0;
	void SetDeathScreen(bool bOn);
	void SetupStatic();
	void UpdateVitals(const UNWCombatComponent* C, float DeltaSeconds);
	void UpdateAbilities(const class ANWCharacter* Me, float DeltaSeconds);
	TMap<FName, TPair<FString, FString>> AbilityArt;   // ação -> (ícone, fundo)
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill);
	void DrawTarget(const APawn* Me);
	void DrawNameplates(const APawn* Me);
	void UpdateConsumables(const class ANWCharacter* Me);
	/** Barra de XP do jogo (hud2024/xp): nível no meio, preenchimento do nível atual. */
	void UpdateXp(const class ANWCharacter* Me);
	int64 XpShown = 0;
	double XpShownAt = -100.0;
	/** Banner de subir de nível (lyshineui/banner/bannerlevelup, parte de XP): runa, faixa, "Subiu de Nível" e o nível. */
	void DrawLevelUp();
	int32 LastLevel = 0;
	int32 LevelUpValue = 0;
	double LevelUpAt = -100.0;
	void DrawLootTicker();
	/**
	 * Bússola do jogo no topo (lyshineui/compass): tira compass2048 rolando com a câmera, linha compassbg e agulha.
	 * Norte do mapa = yaw −4° no L_SouthWindsward (giro da cidade −94°: atan2(cos a, −sin a)).
	 */
	void DrawCompass();
	void DrawDamageNumbers();
	void DrawHitIndicators();
	void DrawWarning();
public:
	/**
	 * Barra de navegação do jogo (lyshineui/navbar, 2023) no alto das telas cheias: Inventário, Personagem e
	 * Mapa, com o selo de pontos livres em Personagem. Active = 0, 1, 2. NavTabAt devolve a aba sob o mouse.
	 */
	void DrawNavbar(int32 Active, const FVector2D& Mouse);
	int32 NavTabAt(const FVector2D& Mouse) const;
private:
	FBox2f NavTabs[3] = {FBox2f(ForceInit), FBox2f(ForceInit), FBox2f(ForceInit)};
	FString WarningText;
	double WarningAt = -100.0;
	struct FHitIndicator { FVector From; float Fraction = 0.f; double Time = 0.0; };
	TArray<FHitIndicator> HitIndicators;
	/** Bolsa na barra de ações (hud2024/actionbar/encumbrance): % do peso; sobrecarregado fica vermelho com o aviso do jogo. */
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
	/** HUD do jogo (tudo menos o menu do Esc, que o DrawHUD desenha por cima). */
	void DrawGameHUD();
	/** Coleta: "[E] Minerar Filão de Ferro" perto de um coletável e a barra durante a coleta. */
	void DrawGather(const APawn* Me);
	/** Cartão de interagir (NPC, estação...: NWInteractable.h) e o ícone do jogo sobre o NPC com brinde. */
	void DrawInteract(const APawn* Me);
	void DrawNpcNames(const APawn* Me);
	/** Faixa do nome à direita do anel e a linha [E] ação (ou aviso em vermelho), do cartão unifiedinteract. */
	void DrawCardText(const FVector2D& Ring, const FString& Name, const FString& Action, bool bBlocked);
	// Linha do aviso de saque (hud2024/lootticker): item (ícone, raridade, tier, quantidade) ou moedas.
	// Linha do aviso: item, moedas ou XP de ofício (Trade = NWTradeskill, Quantity = XP).
	struct FLootLine { FName ItemId; int32 Quantity = 0; uint8 Rarity = 0; int64 Coins = 0; double Time = 0.0; int32 Trade = -1; };
	TArray<FLootLine> LootLines;
	// Membros do grupo à esquerda, com a barra de membro do jogo (Data/UI/group_layout.json).
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

	// Barra "atrasada" (UnderFill) que desce devagar depois do dano, como no jogo.
	float HealthLag = 1.f;
	float StaminaLag = 1.f;
	float LastHealth = -1.f;
	TArray<float> ReadyFlash;   // por slot: tempo restante do brilho de "pronta"
	TArray<bool> WasCooling;
	double LastDrawTime = 0.0;
};
