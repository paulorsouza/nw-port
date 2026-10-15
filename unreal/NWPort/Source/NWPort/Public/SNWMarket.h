#pragma once

#include "CoreMinimal.h"
#include "NWItems.h"
#include "NWMarket.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ANWCharacter;
class SBox;
class SEditableTextBox;
class SToolTip;
class SWidgetSwitcher;

/**
 * Entreposto comercial (esteira 4): primeira tela própria do port, em Slate, com as cores e fontes do jogo
 * (docs/game/city.md §5). Abas Comprar (pedidos de venda dos outros, filtros e tabela ordenável), Vender (itens da bolsa
 * que vão ao mercado) e Meus Pedidos. Pedido de compra fica fora da tela (dono 2026-10-14); o servidor ainda aceita.
 *
 * Resposta otimista: comprar, vender e cancelar mudam a tela na hora (linha nova com spinner, linha some, moedas caem) e
 * o servidor confirma depois (UNWInteractorComponent::ClientMarketDone). Recusado, volta ao que era e a linha pisca em
 * vermelho. A previsão vale até o livro e a bolsa do servidor chegarem.
 */
class NWPORT_API SNWMarket : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNWMarket) : _Character(nullptr), _bShot(false) {}
		SLATE_ARGUMENT(ANWCharacter*, Character)
		SLATE_ARGUMENT(FString, Title)
		/** Captura offline (NWUIShotCommandlet): sem servidor, linhas já assentadas. */
		SLATE_ARGUMENT(bool, bShot)
	SLATE_END_ARGS()

	/** Linha das tabelas: pedido do livro (Comprar, Meus Pedidos) ou item da bolsa (Vender). */
	struct FRow
	{
		FString Key;            // id do pedido, "pending:<n>" ou "bag:<espaço>"
		FNWMarketRow Order;
		FNWItem Item;
		int64 Lowest = 0;       // Vender: menor preço anunciado do mesmo item
		bool bPending = false;  // pedido criado esperando o servidor
		bool bCancelling = false;
		double Born = 0.0;
	};
	using FRowPtr = TSharedPtr<FRow>;

	void Construct(const FArguments& Args);
	virtual ~SNWMarket() override;
	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;

	// Captura offline.
	void ShotTab(int32 Tab);
	void ShotSelect(int32 Index);
	void ShotSearch(const FString& Query);
	void ShotPerkFilter(FName Perk);

	double Clock() const;
	double FlashOf(const FString& Key) const;

private:
	enum class EPage : uint8 { Buy, Sell, Orders };
	enum class ECat : uint8 { All, Weapons, Armor, Jewelry, Tools, Gems, Materials, Num };
	enum class EKind : uint8 { Buy, Post, Cancel };
	struct FPending
	{
		int32 Req = 0;
		EKind Kind = EKind::Buy;
		FString OrderId;
		int32 Slot = INDEX_NONE;
		FNWItem SlotBefore;
		FNWMarketRow Row;
		int32 Qty = 0;
		int64 Coins = 0;
		int64 CoinsBefore = 0;
		double At = 0.0;
		bool bDone = false;
		double DoneAt = 0.0;
	};

	TWeakObjectPtr<ANWCharacter> Character;
	FString Title;
	bool bShot = false;
	double ShotNow = 1000.0;
	EPage Page = EPage::Buy;
	double PageAt = 0.0;
	int32 OpenSeq = 0;
	int32 SeenSeq = -1;
	uint32 SeenBag = 0;
	double LastRefresh = 0.0;
	bool bDirty = true;

	// Filtros (Comprar) e busca (Comprar e Vender).
	FString Search;
	ECat Cat = ECat::All;
	TSet<int32> Tiers;
	TSet<int32> Rarities;
	int32 MinGs = 0;
	TSet<uint8> Attrs;
	TSet<FName> PerkFilter;
	bool bAfford = false;
	TArray<FName> BookPerks;        // perks que aparecem nos pedidos à venda
	uint32 BookPerksSig = 0;

	// Ordenação por página.
	FName SortCol[3];
	EColumnSortMode::Type SortMode[3] = {EColumnSortMode::Ascending, EColumnSortMode::Ascending, EColumnSortMode::Ascending};

	// Linhas (a mesma FRow por chave entre refeitas: a tabela mantém seleção e não reanima).
	TArray<FRowPtr> Rows[3];
	TMap<FString, FRowPtr> Known;
	TSharedPtr<SListView<FRowPtr>> Lists[3];
	FString Selected[3];
	double SelectedAt = 0.0;

	// Painel do item à direita (Comprar e Vender).
	TSharedPtr<SBox> Panels[2];
	TSharedPtr<SEditableTextBox> PriceBox;
	TSharedPtr<SBox> PerkChips;
	int32 Count = 1;
	int64 Price = 0;
	int32 Days = 3;

	TArray<FPending> Pending;
	int32 NextReq = 1;
	TMap<FString, double> Flash;

	// Montagem.
	TSharedRef<SWidget> BuildTabs();
	TSharedRef<SWidget> BuildFilters();
	TSharedRef<SWidget> BuildList(EPage P);
	TSharedRef<SWidget> BuildSearch();
	TSharedRef<SWidget> BuildPerkChips();
	TSharedRef<SWidget> BuildPanel(EPage P);
	TSharedRef<SWidget> ItemHeader(const FNWItem& Item);
	TSharedRef<SWidget> ItemDetails(const FNWItem& Item);
	TSharedRef<SWidget> Stepper(TFunction<int32()> Max);
	TSharedRef<SWidget> Spinner(float Size);
	TSharedRef<SToolTip> ItemTip(const FNWItem& Item);
	void Close();
	TSharedRef<ITableRow> OnRow(FRowPtr Row, const TSharedRef<STableViewBase>& Owner, EPage P);

	// Estado.
	void SetPage(EPage P);
	void Consume();
	void Rebuild();
	void RefreshPanel(EPage P);
	bool Loading() const;
	FRowPtr Find(EPage P, const FString& Key) const;
	FRowPtr Row(const FString& Key, double Now);
	bool Pass(const FNWMarketRow& O) const;
	void Sort(EPage P);
	int64 ShownCoins() const;
	int32 MyOrders() const;
	FNWItem ShownBag(const FNWItem& I) const;
	bool AnyWaiting() const;

	// Ações.
	void Buy(FRowPtr R);
	void PostSell();
	void Cancel(FRowPtr R);
};
