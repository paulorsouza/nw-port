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
	static constexpr float BaseMaxWeight = 1000.f;
	// Peso máximo deste personagem: o base, ou 10.000.000 com a bolsa de dev (F10, não salva).
	UPROPERTY(Replicated) float MaxWeight = BaseMaxWeight;
	// Vida máxima do jogo no nível 60, para escalar a cura das poções à nossa (MaxHealth).
	static constexpr float GameMaxHealth = 7000.f;

	UPROPERTY(Replicated) TArray<FNWItem> Items;
	UPROPERTY(Replicated) int64 Coins = 0;
	// Recarga das poções (tempo do servidor).
	UPROPERTY(Replicated) float PotionReadyAt = 0.f;

	const FNWItem* At(ENWItemContainer Container, int32 Slot) const;
	int32 CountOf(FName ItemId) const;
	/** Servidor: tira até Quantity do item das pilhas da bolsa (refino, NWCrafting); devolve quanto tirou. */
	int32 Take(FName ItemId, int32 Quantity);
	/** Servidor: tira até Quantity da pilha no espaço BagSlot da bolsa (armazém); devolve o que saiu (Quantity 0 = nada). */
	FNWItem TakeAt(int32 BagSlot, int32 Quantity);
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
	/** Moedas em centavos como no jogo (common.lua GetLocalizedCurrency: ÷100, 2 casas, separadores do idioma). */
	static FString FormatCoins(int64 Cents);
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
	/** Destrói Quantity da pilha (largar no chão: o port não tem saco de saque). */
	UFUNCTION(Server, Reliable) void ServerDestroy(int32 BagSlot, int32 Quantity);
	/**
	 * Encaixa a gema ou o vidro rúnico do espaço GemBagSlot da bolsa no item (bolsa ou equipado; docs/itens.md "Slot").
	 * Só em slot vazio: com gema, recusa e avisa (retirar antes, pagando).
	 */
	UFUNCTION(Server, Reliable) void ServerSocket(int32 GemBagSlot, ENWItemContainer Container, int32 Slot);
	/** Retira a gema do item para a bolsa, pagando o custo (FNWItemRules::UnsocketCost). */
	UFUNCTION(Server, Reliable) void ServerUnsocket(ENWItemContainer Container, int32 Slot);
	/** Saque recebido (para o aviso na tela). */
	UFUNCTION(Client, Reliable) void ClientLoot(const TArray<FNWItem>& Gained, int64 CoinsGained);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FNWItem* Find(ENWItemContainer Container, int32 Slot);
	/** Servidor: devolve a gema do item à bolsa pagando o custo; chave do erro ou vazio (nada muda no erro). */
	FString Unsocket(ENWItemContainer Container, int32 Slot);
	void Warn(const TCHAR* Key) const;
	int32 FreeBagSlot() const;
	bool CanEquip(const FNWItem& Item, int32 EquipSlot) const;
	void Changed();
	float ServerNow() const;
};
