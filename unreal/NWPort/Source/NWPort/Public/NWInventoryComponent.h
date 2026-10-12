#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NWItems.h"
#include "NWInventoryComponent.generated.h"

/**
 * Inventário do jogador: bolsa de BagSize espaços (pilhas pelo catálogo), equipamento (espada, machadinha,
 * escudo e as quatro ferramentas de coleta) e moedas. O servidor é autoritativo; o dono recebe a cópia replicada e pede as mudanças por RPC.
 * Salvo no nw-core junto com o estado do personagem (FNWCharacterRecord).
 */
UCLASS(ClassGroup = (NW), meta = (BlueprintSpawnableComponent))
class NWPORT_API UNWInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNWInventoryComponent();

	static constexpr int32 BagSize = 40;
	// Peso máximo sem sobrecarga (inventário base do jogo); acima disso não corre, não esquiva, não pula.
	static constexpr float MaxWeight = 1000.f;
	// Vida máxima do jogo no nível 60, para escalar a cura das poções à nossa (MaxHealth).
	static constexpr float GameMaxHealth = 7000.f;

	UPROPERTY(Replicated) TArray<FNWItem> Items;
	UPROPERTY(Replicated) int64 Coins = 0;
	// Recarga das poções (tempo do servidor).
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;

	const FNWItem* At(ENWItemContainer Container, int32 Slot) const;
	int32 CountOf(FName ItemId) const;
	/** Poção de vida de maior tier na bolsa (espaço) ou INDEX_NONE. */
	int32 BestPotionSlot() const;
	/** Multiplicador de dano da arma equipada do conjunto (Sword/Hatchet) pelo gear score; 1 sem arma. */
	float DamageMultiplier(FName WeaponSet) const;
	float GetPotionCooldown() const;
	/** Peso total (bolsa + equipado), kg, pelo catálogo (Weight do jogo). Servidor e dono têm os itens. */
	float TotalWeight() const;
	bool IsEncumbered() const { return TotalWeight() > MaxWeight; }

	// ---- servidor
	/** Põe na bolsa (junta pilhas, depois espaços livres). Devolve o que não coube. */
	int32 AddItem(const FNWItem& Item);
	void AddCoins(int64 Amount);
	/** Entrega um saque: põe na bolsa, avisa se não coube e mostra o aviso de saque ao dono. */
	void Grant(const TArray<FNWItem>& Gained, int64 Coins);
	void Load(const TArray<FNWItem>& InItems, int64 InCoins);
	void GiveStarterKit();
	/** Ferramentas T1 de coleta nos espaços vazios (kit inicial e menu de dev). */
	void GiveTools();
	/** Ferramenta equipada para o tipo de coleta do jogo (Mining, Cutting, Chopping, Dressing). */
	const FNWItemDef* ToolFor(FName GatherType) const;

	// ---- dono -> servidor
	UFUNCTION(Server, Reliable) void ServerMove(ENWItemContainer FromContainer, int32 FromSlot, ENWItemContainer ToContainer, int32 ToSlot);
	UFUNCTION(Server, Reliable) void ServerUse(int32 BagSlot);
	UFUNCTION(Server, Reliable) void ServerUseBestPotion();
	UFUNCTION(Server, Reliable) void ServerDestroy(int32 BagSlot);
	/** Saque recebido (para o aviso na tela). */
	UFUNCTION(Client, Reliable) void ClientLoot(const TArray<FNWItem>& Gained, int64 CoinsGained);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FNWItem* Find(ENWItemContainer Container, int32 Slot);
	int32 FreeBagSlot() const;
	bool CanEquip(const FNWItem& Item, int32 EquipSlot) const;
	void Changed();
	float ServerNow() const;
};
