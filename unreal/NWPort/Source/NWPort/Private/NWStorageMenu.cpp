#include "NWStorageMenu.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "NWCharacter.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWText.h"
#include "NWUILayout.h"

namespace
{
	const FLinearColor Tan(FColor(0xEB, 0xE3, 0xD6));
	const FLinearColor Dim(FColor(0x9A, 0x96, 0x8E));
	const FLinearColor Red(FColor(0xFF, 0x71, 0x71));
	const TCHAR* Img = TEXT("lyshineui/images/");
	constexpr int32 Rows = 13;
	constexpr float RowH = 56.f;
}

void FNWStorageMenu::Open(ANWCharacter* InCharacter, const FString& InTitle)
{
	Character = InCharacter;
	Title = InTitle;
	ScrollBag = ScrollStore = 0;
}

void FNWStorageMenu::Close()
{
	if (ANWCharacter* Me = Character.Get(); Me && Me->Interactor)
	{
		Me->Interactor->ServerStorageClose();
	}
	Character.Reset();
}

void FNWStorageMenu::Text(UCanvas* Canvas, const FString& T, float X, float Y, float Size, const FLinearColor& Color, int32 Style, bool bRight) const
{
	UFont* Font = FNWUILayout::GameFont(Style);
	if (!Font || T.IsEmpty())
	{
		return;
	}
	const float Scale = Size * S / Font->LegacyFontSize;
	FVector2D Pos = P(X, Y);
	if (bRight)
	{
		Pos.X -= Font->GetStringSize(*T) * Scale;
	}
	FCanvasTextItem Item(Pos, FText::FromString(T), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	Canvas->DrawItem(Item);
}

void FNWStorageMenu::Tex(UCanvas* Canvas, const FString& Path, const FBox2f& Box, const FLinearColor& Tint) const
{
	if (UTexture2D* T = FNWUILayout::GameTexture(Path))
	{
		FCanvasTileItem Tile(FVector2D(Box.Min), T->GetResource(), FVector2D(Box.GetSize()), Tint);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

void FNWStorageMenu::Column(UCanvas* Canvas, const FVector2D& Mouse, float X, const FString& Header, const TArray<const FNWItem*>& Items,
	float Weight, float Capacity, int32& Scroll, TArray<TPair<FBox2f, int32>>& RowsOut)
{
	Tex(Canvas, FString(Img) + TEXT("slices/framemultibg/framestyles/framebgdefaultnooutline.dds"), R(X, 130, X + 620, 1010));
	Tex(Canvas, FString(Img) + TEXT("slices/frameheader/headerbg.dds"), R(X, 92, X + 620, 150));
	Text(Canvas, Header.ToUpper(), X + 25, 104, 24, Tan, 2);
	Text(Canvas, FString::Printf(TEXT("%.1f / %.0f"), Weight, Capacity), X + 600, 108, 18, Weight > Capacity ? Red : Dim, 1, true);
	Scroll = FMath::Clamp(Scroll, 0, FMath::Max(0, Items.Num() - Rows));
	for (int32 i = Scroll; i < Items.Num() && i < Scroll + Rows; ++i)
	{
		const FNWItem& It = *Items[i];
		const float Y = 170.f + (i - Scroll) * RowH;
		const FBox2f Row = R(X + 10, Y, X + 610, Y + RowH - 4);
		RowsOut.Add({Row, It.Slot});
		if (Row.IsInside(FVector2f(Mouse)))
		{
			Tex(Canvas, FString(Img) + TEXT("slices/listitembg/listitemfocusadd.dds"), Row, FLinearColor(1.f, 1.f, 1.f, 0.35f));
		}
		Tex(Canvas, FString(Img) + TEXT("slices/itemlayout/itembgsquare0.dds"), R(X + 18, Y + 4, X + 62, Y + 48));
		const FNWItemDef* D = FNWItemCatalog::Get().Find(It.ItemId);
		if (D && !D->Icon.IsEmpty())
		{
			Tex(Canvas, D->Icon, R(X + 21, Y + 7, X + 59, Y + 45));
		}
		Text(Canvas, D ? D->Name : It.ItemId.ToString(), X + 75, Y + 8, 18, D ? NWRarityColor(It.Rarity) : Tan);
		Text(Canvas, FString::Printf(TEXT("%d"), It.Quantity), X + 600, Y + 14, 18, Tan, 1, true);
	}
}

void FNWStorageMenu::Draw(UCanvas* Canvas, const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	UNWInteractorComponent* I = Me ? Me->Interactor.Get() : nullptr;
	if (!Me || !I || !Me->Inventory || !Canvas)
	{
		Character.Reset();
		return;
	}
	S = Canvas->ClipY / 1080.f;
	Origin = FBox2f(FVector2f((Canvas->ClipX - 1920.f * S) * 0.5f, 0.f), FVector2f((Canvas->ClipX + 1920.f * S) * 0.5f, Canvas->ClipY));
	Tex(Canvas, FString(Img) + TEXT("crafting/crafting_background_step3.dds"), R(0, 0, 1920, 1080), FLinearColor(1.f, 1.f, 1.f, 0.92f));
	TArray<const FNWItem*> Bag, Store;
	for (const FNWItem& It : Me->Inventory->Items)
	{
		if (It.Container == ENWItemContainer::Bag)
		{
			Bag.Add(&It);
		}
	}
	for (const FNWItem& It : I->StorageItems)
	{
		Store.Add(&It);
	}
	Bag.Sort([](const FNWItem& A, const FNWItem& B) { return A.Slot < B.Slot; });
	Store.Sort([](const FNWItem& A, const FNWItem& B) { return A.Slot < B.Slot; });
	BagRows.Reset();
	StoreRows.Reset();
	Column(Canvas, Mouse, 250, NWText::Get(TEXT("nw.storage.bag")), Bag, Me->Inventory->TotalWeight(), Me->Inventory->MaxWeight, ScrollBag, BagRows);
	Column(Canvas, Mouse, 1050, Title, Store, I->StorageWeight(), I->StorageCapacity, ScrollStore, StoreRows);
}

bool FNWStorageMenu::Click(const FVector2D& Mouse)
{
	ANWCharacter* Me = Character.Get();
	if (!Me || !Me->Interactor)
	{
		return false;
	}
	const FVector2f M(Mouse);
	for (const TPair<FBox2f, int32>& Row : BagRows)
	{
		if (Row.Key.IsInside(M))
		{
			const FNWItem* It = Me->Inventory->At(ENWItemContainer::Bag, Row.Value);
			Me->Interactor->ServerStorageMove(true, Row.Value, It ? It->Quantity : 0);
			return true;
		}
	}
	for (const TPair<FBox2f, int32>& Row : StoreRows)
	{
		if (Row.Key.IsInside(M))
		{
			const FNWItem* It = Me->Interactor->StorageItems.FindByPredicate([&Row](const FNWItem& X) { return X.Slot == Row.Value; });
			Me->Interactor->ServerStorageMove(false, Row.Value, It ? It->Quantity : 0);
			return true;
		}
	}
	return true;
}

void FNWStorageMenu::Wheel(float Delta, const FVector2D& Mouse)
{
	int32& Scroll = Mouse.X < Origin.Min.X + 960.f * S ? ScrollBag : ScrollStore;
	Scroll = FMath::Max(0, Scroll - int32(Delta) * 3);
}
