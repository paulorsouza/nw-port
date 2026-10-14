#include "NWUIShotCommandlet.h"

#include "CanvasTypes.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "EngineFontServices.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInventoryComponent.h"
#include "NWProgressionComponent.h"
#include "NWRefiningMenu.h"
#include "NWInventoryMenu.h"
#include "NWInteractable.h"
#include "NWNpcRewards.h"
#include "NWShopMenu.h"
#include "NWUILayout.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWUIShot, Log, All);

UNWUIShotCommandlet::UNWUIShotCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace
{
	// XP mínimo para o nível do ofício (as tabelas ficam no componente; busca pelo próprio TradeLevel).
	void SetTradeLevel(UNWProgressionComponent* P, int32 Skill, int32 Level)
	{
		int32 Lo = 0, Hi = 1;
		P->TradeXp[Skill] = Hi;
		while (P->TradeLevel(Skill) < Level && Hi < (1 << 29))
		{
			Hi *= 2;
			P->TradeXp[Skill] = Hi;
		}
		while (Lo < Hi)
		{
			const int32 Mid = Lo + (Hi - Lo) / 2;
			P->TradeXp[Skill] = Mid;
			if (P->TradeLevel(Skill) >= Level) { Hi = Mid; } else { Lo = Mid + 1; }
		}
		P->TradeXp[Skill] = Lo;
	}

	// Texturas da UI chegam ao render target só depois de compiladas e com todos os mips.
	void SettleTextures()
	{
#if WITH_EDITOR
		FTextureCompilingManager::Get().FinishAllCompilation();   // só existe no editor (o servidor compila o arquivo também)
#endif
		for (TObjectIterator<UTexture2D> It; It; ++It)
		{
			It->SetForceMipLevelsToBeResident(30.f);
		}
		IStreamingManager::Get().StreamAllResources(5.f);
		// Glifos das fontes de runtime: o atlas sobe para a GPU no tick do motor, que o commandlet não tem.
		if (FEngineFontServices::IsInitialized())
		{
			FEngineFontServices::Get().UpdateCache();
		}
		FlushRenderingCommands();
	}
}

int32 UNWUIShotCommandlet::Main(const FString& Params)
{
	FString File, OutDir;
	FParse::Value(*Params, TEXT("scenario="), File);
	if (!FParse::Value(*Params, TEXT("out="), OutDir))
	{
		OutDir = FPaths::ProjectDir() / TEXT("../../extract/ui/shots");
	}
	FString Text;
	TSharedPtr<FJsonObject> S;
	if (!FFileHelper::LoadFileToString(Text, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), S) || !S)
	{
		UE_LOG(LogNWUIShot, Error, TEXT("cenário inválido: %s"), *File);
		return 1;
	}
	const FString Screen = S->GetStringField(TEXT("screen"));
	if (Screen != TEXT("refining") && Screen != TEXT("shop") && Screen != TEXT("inventory"))
	{
		UE_LOG(LogNWUIShot, Error, TEXT("tela sem captura: %s"), *Screen);
		return 1;
	}

	// Fontes de runtime medem e desenham pelo cache da Slate, que o commandlet sobe sem renderizador.
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
	}
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetRenderer())
	{
		FSlateApplication::Get().InitializeRenderer(
			FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer(), true);
	}
	UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot slate: app %d, renderer %d"), FSlateApplication::IsInitialized(),
		FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr);
	if (FEngineFontServices::IsInitialized())
	{
		FEngineFontServices::Destroy();
	}
	FEngineFontServices::Create();
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("NWUIShot"));
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
	Ctx.SetCurrentWorld(World);
	World->AddToRoot();

	// Sem construção (malha, anim): a tela só lê bolsa, níveis e o componente de interação.
	FActorSpawnParameters Deferred;
	Deferred.bDeferConstruction = true;
	ANWCharacter* Me = World->SpawnActor<ANWCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Deferred);
	ANWStation* St = World->SpawnActor<ANWStation>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Me || !St)
	{
		UE_LOG(LogNWUIShot, Error, TEXT("não criou personagem/estação"));
		return 1;
	}
	if (S->HasField(TEXT("station")))
	{
		const TSharedPtr<FJsonObject> SO = S->GetObjectField(TEXT("station"));
		FNWCrafting::FStation Data;
		Data.Type = SO->GetStringField(TEXT("type"));
		Data.Tier = int32(SO->GetNumberField(TEXT("tier")));
		for (const FString Lang : {TEXT("pt-br"), TEXT("en-us")})
		{
			Data.Name.Add(Lang, SO->GetStringField(TEXT("name")));
		}
		St->Setup(Data);
	}

	TArray<FNWItem> Items;
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("items")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		FNWItem I;
		I.ItemId = FName(A[0]->AsString());
		I.Quantity = int32(A[1]->AsNumber());
		I.Slot = Items.Num();
		I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
		I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
		if (A.Num() > 4 && A[4]->Type == EJson::Object)
		{
			NWItemProps::Read(I, A[4]->AsObject());   // props do item fabricado (qualidade, perks, gema...)
		}
		Items.Add(I);
	}
	// Inventário: equipamento [[item, espaço, gs, raridade]] e armazém [[item, qtd, gs, raridade]].
	const TArray<TSharedPtr<FJsonValue>>* Equip = nullptr;
	if (S->TryGetArrayField(TEXT("equip"), Equip))
	{
		for (const TSharedPtr<FJsonValue>& V : *Equip)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			FNWItem I;
			I.ItemId = FName(A[0]->AsString());
			I.Quantity = 1;
			I.Container = ENWItemContainer::Equip;
			I.Slot = int32(A[1]->AsNumber());
			I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
			I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
			if (A.Num() > 4 && A[4]->Type == EJson::Object)
			{
				NWItemProps::Read(I, A[4]->AsObject());
			}
			Items.Add(I);
		}
	}
	TArray<FNWItem> Stored;
	const TArray<TSharedPtr<FJsonValue>>* StoredArr = nullptr;
	if (S->TryGetArrayField(TEXT("storage"), StoredArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *StoredArr)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			FNWItem I;
			I.ItemId = FName(A[0]->AsString());
			I.Quantity = int32(A[1]->AsNumber());
			I.Slot = Stored.Num();
			I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
			I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
			Stored.Add(I);
		}
	}
	if (Me->Interactor)
	{
		Me->Interactor->StorageItems = Stored;
		if (S->HasField(TEXT("storage_capacity")))
		{
			Me->Interactor->StorageCapacity = float(S->GetNumberField(TEXT("storage_capacity")));
		}
	}
	Me->Inventory->Load(Items, int64(S->GetNumberField(TEXT("coins"))));
	Me->Progression->bFromBackend = true;   // exige o nível do ofício, como no jogo com backend
	// Loja: compras da semana já feitas (limite semanal), {"FluxT5": 100}.
	const TSharedPtr<FJsonObject>* Bought = nullptr;
	if (S->TryGetObjectField(TEXT("bought"), Bought))
	{
		for (const auto& B : (*Bought)->Values)
		{
			Me->Progression->AddShopBought(FName(*FString(B.Key)), int32(B.Value->AsNumber()), FNWNpcRewards::Get().Period());
		}
	}
	const TSharedPtr<FJsonObject>* Trade = nullptr;
	const TSharedPtr<FJsonObject> NoTrade = MakeShared<FJsonObject>();
	for (const auto& T : (S->TryGetObjectField(TEXT("trade"), Trade) ? *Trade : NoTrade)->Values)
	{
		const int32 Skill = NWTradeskill::Index(FName(*FString(T.Key)));
		if (Skill != INDEX_NONE)
		{
			SetTradeLevel(Me->Progression, Skill, int32(T.Value->AsNumber()));
		}
	}

	FString Only;
	FParse::Value(*Params, TEXT("shot="), Only);
	const FString Base = FPaths::GetBaseFilename(File);
	int32 Saved = 0;
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("shots")))
	{
		const TSharedPtr<FJsonObject> Shot = V->AsObject();
		const FString Name = Shot->GetStringField(TEXT("name"));
		if (!Only.IsEmpty() && Only != Name)
		{
			continue;
		}
		const int32 W = Shot->HasField(TEXT("w")) ? int32(Shot->GetNumberField(TEXT("w"))) : 1920;
		const int32 H = Shot->HasField(TEXT("h")) ? int32(Shot->GetNumberField(TEXT("h"))) : 1080;
		FVector2D Mouse(-100.0, -100.0);
		const TArray<TSharedPtr<FJsonValue>>* MouseArr = nullptr;
		if (Shot->TryGetArrayField(TEXT("mouse"), MouseArr) && MouseArr->Num() == 2)
		{
			Mouse = FVector2D((*MouseArr)[0]->AsNumber(), (*MouseArr)[1]->AsNumber());
		}

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>();
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor(FColor(0x1E, 0x1C, 0x19));   // fundo neutro escuro (no jogo, o mundo atrás)
		RT->InitAutoFormat(W, H);
		RT->UpdateResourceImmediate(true);
		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();

		Me->Inventory->Load(Items, int64(S->GetNumberField(TEXT("coins"))));   // bolsa do cenário a cada captura (o roll refina de verdade)
		FNWRefiningMenu Menu;
		FNWShopMenu Shop;
		FNWInventoryMenu Inventory;
		const bool bShop = Screen == TEXT("shop");
		const bool bInventory = Screen == TEXT("inventory");
		Menu.FixedNow = Shop.FixedNow = 1000.0;
		const int32 Count = Shot->HasField(TEXT("count")) ? int32(Shot->GetNumberField(TEXT("count"))) : 1;
		if (bInventory)
		{
			bool bStorageShot = false;
			Shot->TryGetBoolField(TEXT("storage"), bStorageShot);
			bStorageShot ? Inventory.OpenStorage(Me, S->GetStringField(TEXT("storage_title"))) : Inventory.Open(Me);
			// Ordenação: {"sort": 1..4} na lista do inventário, {"storage_sort": 1..4} na do armazém.
			if (Shot->HasField(TEXT("sort")))
			{
				Inventory.ShotSort(0, int32(Shot->GetNumberField(TEXT("sort"))));
			}
			if (Shot->HasField(TEXT("storage_sort")))
			{
				Inventory.ShotSort(1, int32(Shot->GetNumberField(TEXT("storage_sort"))));
			}
			// Busca: {"search": texto} na bolsa, {"storage_search": texto} no armazém.
			FString Query;
			if (Shot->TryGetStringField(TEXT("search"), Query))
			{
				Inventory.ShotSearch(0, Query);
			}
			if (Shot->TryGetStringField(TEXT("storage_search"), Query))
			{
				Inventory.ShotSearch(1, Query);
			}
			// Popup: {"popup": {"kind": "split|destroy|socket|unsocket", "from": "bag|equip|storage", "item": Id, "qty": n,
			// "gem": Id (socket: gema na bolsa)}}.
			const TSharedPtr<FJsonObject>* Pop = nullptr;
			if (Shot->TryGetObjectField(TEXT("popup"), Pop))
			{
				const FString From = (*Pop)->GetStringField(TEXT("from"));
				const int32 Src = From == TEXT("storage") ? 2 : From == TEXT("equip") ? 1 : 0;
				const FName Id((*Pop)->GetStringField(TEXT("item")));
				const TArray<FNWItem>& Pool = Src == 2 ? Me->Interactor->StorageItems : Me->Inventory->Items;
				const ENWItemContainer Cont = Src == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag;
				if (const FNWItem* Found = Pool.FindByPredicate([&](const FNWItem& I) { return I.ItemId == Id && (Src == 2 || I.Container == Cont); }))
				{
					const FString Kind = (*Pop)->GetStringField(TEXT("kind"));
					int32 Qty = (*Pop)->HasField(TEXT("qty")) ? int32((*Pop)->GetNumberField(TEXT("qty"))) : 0;
					FString Gem;
					if (Kind == TEXT("socket") && (*Pop)->TryGetStringField(TEXT("gem"), Gem))
					{
						const FNWItem* G = Me->Inventory->Items.FindByPredicate([&](const FNWItem& I) { return I.ItemId == FName(*Gem) && I.Container == ENWItemContainer::Bag; });
						Qty = G ? G->Slot : INDEX_NONE;
					}
					const int32 K = Kind == TEXT("destroy") ? 2 : Kind == TEXT("socket") ? 3 : Kind == TEXT("unsocket") ? 4 : 1;
					Inventory.ShotPopup(K, Src, Found->Slot, Qty);
				}
			}
			// Arrasto simulado: {"from": "bag|equip|storage", "item": Id}.
			const TSharedPtr<FJsonObject>* Drag = nullptr;
			if (Shot->TryGetObjectField(TEXT("drag"), Drag))
			{
				const FString From = (*Drag)->GetStringField(TEXT("from"));
				const FName Id((*Drag)->GetStringField(TEXT("item")));
				const int32 Src = From == TEXT("equip") ? 1 : From == TEXT("storage") ? 2 : 0;
				const TArray<FNWItem>& Pool = Src == 2 ? Me->Interactor->StorageItems : Me->Inventory->Items;
				const FNWItem* Found = Pool.FindByPredicate([&](const FNWItem& I)
				{
					return I.ItemId == Id && (Src == 2 || I.Container == (Src == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag));
				});
				if (Found)
				{
					Inventory.ShotDrag(Src, Found->Slot);
				}
			}
		}
		else if (bShop)
		{
			Shop.Open(Me, S->GetStringField(TEXT("shop")), S->GetStringField(TEXT("title")));
			FString Item;
			if (Shot->TryGetStringField(TEXT("item"), Item))
			{
				Shop.Select(FName(Item), Count);
			}
		}
		else
		{
			Menu.Open(Me, St);
			FString Recipe;
			if (Shot->TryGetStringField(TEXT("recipe"), Recipe))
			{
				Menu.Select(FName(Recipe), Count);
			}
			const TSharedPtr<FJsonObject>* Improve = nullptr;
			if (Shot->TryGetObjectField(TEXT("improve"), Improve))   // aba "Melhorar itens": {"slot": espaço da bolsa, "mat": material}
			{
				Menu.ShowImprove((*Improve)->HasField(TEXT("slot")) ? int32((*Improve)->GetNumberField(TEXT("slot"))) : INDEX_NONE,
					(*Improve)->HasField(TEXT("mat")) ? uint8((*Improve)->GetNumberField(TEXT("mat"))) : 0);
			}
		}

		auto Frame = [&]()
		{
			FCanvas Canvas(Res, nullptr, FGameTime::CreateUndilated(Menu.FixedNow, 0.0), World->GetFeatureLevel());
			Canvas.Clear(RT->ClearColor);
			UCanvas* C = NewObject<UCanvas>();
			C->Init(W, H, nullptr, &Canvas);
			C->Update();
			bInventory ? Inventory.Draw(C, Mouse) : bShop ? Shop.Draw(C, Mouse) : Menu.Draw(C, Mouse);
			Canvas.Flush_GameThread(true);
			FlushRenderingCommands();
		};
		// Primeiro quadro carrega as texturas e mede as linhas; o segundo é o que vale.
		Frame();
		SettleTextures();
		Frame();
		if (!bShop && !bInventory && Shot->HasField(TEXT("craft")) && Shot->GetBoolField(TEXT("craft")))
		{
			Menu.ClickCraft();
			Frame();   // o item do equipamento chega aqui (t0 do roll); t conta a partir dele
			Menu.FixedNow += Shot->HasField(TEXT("t")) ? Shot->GetNumberField(TEXT("t")) : 1.0;
			Frame();
			SettleTextures();
			Frame();
		}

		FString DumpFilter;
		if (FParse::Value(*Params, TEXT("dump="), DumpFilter))
		{
			bShop ? Shop.Dump(DumpFilter) : Menu.Dump(DumpFilter);
		}
		TArray<FColor> Pixels;
		Res->ReadPixels(Pixels);
		for (FColor& P : Pixels)
		{
			P.A = 255;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString Out = FPaths::ConvertRelativePathToFull(OutDir / FString::Printf(TEXT("%s_%s.png"), *Base, *Name));
		if (FFileHelper::SaveArrayToFile(Png, *Out))
		{
			UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot: %s"), *Out);
			++Saved;
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot: %d capturas"), Saved);
	return Saved > 0 ? 0 : 1;
}
